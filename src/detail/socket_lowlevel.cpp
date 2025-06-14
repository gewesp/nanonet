//
// C++ wrappers around low-level socket operations
//

#include "cpp-lib/detail/socket_lowlevel.h"

#include "cpp-lib/assert.h"
#include "cpp-lib/detail/platform_wrappers.h"

#include "cpp-lib/detail/platform_definition.h"

using nanonet::detail_::socketfd_t;

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
  ) { nanonet::detail_::throw_socket_error( "setsockopt" ) ; }

}

} // anonymous namespace


void nanonet::detail_::throw_socket_error(const std::string& msg) {
  nanonet::detail_::strerror_exception(msg);
}

void nanonet::detail_::bool_sockopt( 
    socketfd_t const fd , int const option , bool const enable ) {
  ::enable_sockopt<int>( fd , option , enable ) ;
}

void nanonet::detail_::time_sockopt(
    socketfd_t const fd , int const option , double const t ) {
  assert( SO_SNDTIMEO == option || SO_RCVTIMEO == option ) ;

  ::timeval const tv = nanonet::detail_::to_timeval( t ) ;
  ::enable_sockopt<::timeval>( fd , option , tv ) ;
}

void nanonet::detail_::bool_fcntl_option(
    const socketfd_t fd, const int option, const std::string& error_msg, 
    const bool enable) {
  const int previous_flags = fcntl(fd, F_GETFL);

  if (-1 == previous_flags) {
    nanonet::detail_::throw_socket_error(error_msg + ": reading options");
  }
  
  const int arg = enable ? (previous_flags |  option) 
                         : (previous_flags & ~option);
  const int res = fcntl(fd, F_SETFL, arg);
  if (-1 == res) {
    nanonet::detail_::throw_socket_error(error_msg + ": setting options");
  }
} 

int nanonet::detail_::get_sockopt_error(const socketfd_t fd) {
  // The error value to query and return
  int ret = 0;
  socklen_t ret_size = sizeof(int);
  const int res = ::getsockopt(fd, SOL_SOCKET, SO_ERROR, &ret, &ret_size);
  if (-1 == res) {
    nanonet::detail_::throw_socket_error("getsockopt() for error");
  }

  always_assert(ret_size == sizeof(int));
  return ret;
}

int nanonet::detail_::socketsend( socketfd_t const fd , char const* const data , 
    std::size_t const size ) {
#if (BOOST_OS_LINUX)
  const int flags = MSG_NOSIGNAL ;
#else
  const int flags = 0 ;
#endif
  return send( fd , data , size , flags ) ;
}

void nanonet::detail_::setup_stream_socket( socketfd_t const fd ) {
#if (BOOST_OS_MACOS)
  bool_sockopt( fd , SO_NOSIGPIPE ) ;
#else
  static_cast<void>(fd);
#endif
}
