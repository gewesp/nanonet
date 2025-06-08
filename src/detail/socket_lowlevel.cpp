//
// Copyright 2015 KISS Technologies GmbH, Switzerland
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// 
//     http://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "cpp-lib/detail/socket_lowlevel.h"

#include "cpp-lib/assert.h"
#include "cpp-lib/detail/platform_wrappers.h"

#include "boost/predef.h"

using cpl::detail_::socketfd_t;

namespace {

// Enable certain SO_* options (like SO_REUSEADDR, SO_BROADCAST,...)
// or disable again with false
template< typename T > void enable_sockopt
( socketfd_t const fd , int const option, T const& optval ) {

  if(
    ::setsockopt(
      fd ,
      SOL_SOCKET ,
      option ,
      reinterpret_cast< char const* >( &optval ) ,
      sizeof( optval )
    ) == -1
  ) { cpl::detail_::throw_socket_error( "setsockopt" ) ; }

}

} // anonymous namespace


void cpl::detail_::throw_socket_error(const std::string& msg) {
  cpl::detail_::strerror_exception(msg);
}

void cpl::detail_::bool_sockopt( 
    socketfd_t const fd , int const option , bool const enable ) {
  ::enable_sockopt<int>( fd , option , enable ) ;
}

void cpl::detail_::time_sockopt(
    socketfd_t const fd , int const option , double const t ) {
  assert( SO_SNDTIMEO == option || SO_RCVTIMEO == option ) ;

  ::timeval const tv = cpl::detail_::to_timeval( t ) ;
  ::enable_sockopt<::timeval>( fd , option , tv ) ;
}

void cpl::detail_::bool_fcntl_option(
    const socketfd_t fd, const int option, const std::string& error_msg, 
    const bool enable) {
  const int previous_flags = fcntl(fd, F_GETFL);

  if (-1 == previous_flags) {
    cpl::detail_::throw_socket_error(error_msg + ": reading options");
  }
  
  const int arg = enable ? (previous_flags |  option) 
                         : (previous_flags & ~option);
  const int res = fcntl(fd, F_SETFL, arg);
  if (-1 == res) {
    cpl::detail_::throw_socket_error(error_msg + ": setting options");
  }
} 

int cpl::detail_::get_sockopt_error(const socketfd_t fd) {
  // The error value to query and return
  int ret = 0;
  socklen_t ret_size = sizeof(int);
  const int res = ::getsockopt(fd, SOL_SOCKET, SO_ERROR, &ret, &ret_size);
  if (-1 == res) {
    cpl::detail_::throw_socket_error("getsockopt() for error");
  }

  always_assert(ret_size == sizeof(int));
  return ret;
}

int cpl::detail_::socketsend( socketfd_t const fd , char const* const data , 
    std::size_t const size ) {
#if (BOOST_OS_LINUX)
  const int flags = MSG_NOSIGNAL ;
#else
  const int flags = 0 ;
#endif
  return send( fd , data , size , flags ) ;
}

void cpl::detail_::setup_stream_socket( socketfd_t const fd ) {
#if (BOOST_OS_MACOS)
  bool_sockopt( fd , SO_NOSIGPIPE ) ;
#else
  static_cast<void>(fd);
#endif
}
