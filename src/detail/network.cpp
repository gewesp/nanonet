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

#include "nanonet/detail/network.h"

#include "nanonet/detail/platform_net_impl.h"
#include "nanonet/detail/socket_lowlevel.h"

#include "nanonet/math-util.h"

using nanonet::detail_::socketfd_t;
using nanonet::detail_::invalid_socket;

namespace {

/// Polls fd for events, with timeout [s]
/// @return 0 on timeout, > 0 if fd became ready or has an error.
/// -1 is not returned, instead throws an excption.
int poll_one(
    const socketfd_t fd, 
    const short events, 
    const double timeout, 
    const char* const msg) {
  const int timeout_ms = nanonet::math::round_to_integer<int>(timeout * 1e3);

  struct ::pollfd fds;
  fds.fd      = fd    ;
  fds.events  = events;
  fds.revents = 0     ;

  int res = 0;
  do { res = ::poll(&fds, 1, timeout_ms); }
  while (nanonet::detail_::EINTR_repeat(res));

  if (-1 == res) {
    nanonet::detail_::throw_socket_error(std::string(msg) + ": " + "poll() failed");
  }

  always_assert(res >= 0);

  return res;
}

} // anonymous namespace

void nanonet::detail_::my_listen( socketfd_t const fd , int const backlog ) {

  int ret ;
  do 
  { ret = ::listen( fd , backlog ) ; } 
  while( EINTR_repeat( ret ) ) ;

  if( ret < 0 )
  { throw_socket_error( "listen" ) ; }

}

void nanonet::detail_::my_bind( socketfd_t const fd , const sockaddr* const a, const socklen_t len)
{
  int ret ;
  do { ret = ::bind( fd , a , len ) ; }
  while( EINTR_repeat( ret ) ) ;

  if( ret < 0 )
  { throw_socket_error( "bind" ) ; }
  
}

socketfd_t nanonet::detail_::my_accept( socketfd_t const fd ) {

  // Don't care where the connection request comes from...

  socketfd_t ret ;
  do { ret = ::accept( fd , 0 , 0 ) ; } 
  while( EINTR_repeat( ret ) ) ;

  if( nanonet::detail_::invalid_socket() == ret ) { throw_socket_error( "accept" ) ; }

  return ret ;

}

socketfd_t nanonet::detail_::my_accept(const socketfd_t fd, const double timeout) {
  if (timeout < 0) {
    return nanonet::detail_::my_accept(fd);
  }

  nanonet::detail_::bool_fcntl_option(fd, O_NONBLOCK, "Enabling nonblocking mode", true);

  const int poll_res = ::poll_one(
      fd, POLLIN, timeout, "accepting connections");
  if (0 == poll_res) {
    nanonet::util::throw_timeout_exception();
  }

  // OK, the poll() call succeeded and no timeout occurred.  This is the only
  // thing we're interested in, so we ignore the revents field.

  // May throw if it fails
  const socketfd_t ret = nanonet::detail_::my_accept(fd);

  // Ensure that the returned connected socket is blocking
  nanonet::detail_::bool_fcntl_option(ret, O_NONBLOCK, "Disabling nonblocking mode", false);

  return ret;
}
 
void nanonet::detail_::my_connect(
    socketfd_t const fd , const sockaddr* const a, const socklen_t len)
{  
  int ret ;
  do
  { ret = ::connect( fd , a , len ) ; }
  while( EINTR_repeat( ret ) ) ;

  if( ret < 0 )
  { throw_socket_error( "connect" ) ; }
}

void nanonet::detail_::my_connect(
    socketfd_t const fd, 
    const sockaddr* const a, const socklen_t len, 
    const double timeout)
{
  if (timeout < 0) {
    nanonet::detail_::my_connect(fd, a, len);
    return;
  }

  nanonet::detail_::bool_fcntl_option(fd, O_NONBLOCK, "Enabling nonblocking mode", true);
  
  // Socket is now non-blocking
  int ret = 0;
  do
  { ret = ::connect( fd , a , len ) ; }
  while( EINTR_repeat( ret ) ) ;

  // Success, even though unlikely given that the socket is nonblocking (?)
  if (ret >= 0) {
    return;
  }

  // Errors other than in progress: report and get out of here
  if (EINPROGRESS != errno) {
    throw_socket_error("connect");
  }

  // Poll for ability to write
  const int poll_res = ::poll_one(
      fd, POLLOUT, timeout, "connecting");
  if (0 == poll_res) {
    nanonet::util::throw_timeout_exception(timeout, "connecting");
  }

  // man connect(2):
  // "After select(2) indicates writability, use getsockopt(2) to read 
  // the SO_ERROR option at level SOL_SOCKET to determine whether 
  // connect() completed successfully (SO_ERROR is zero) or unsuccessfully 
  // (SO_ERROR is one of the usual error codes  listed  here, explaining 
  // the reason for the failure).
  const int err = nanonet::detail_::get_sockopt_error(fd);

  if (0 != err) {
    nanonet::detail_::strerror_exception("connect() after poll()", err);
  }

  // All OK, switch back to blocking mode
  nanonet::detail_::bool_fcntl_option(
      fd, O_NONBLOCK, "Enabling nonblocking mode", false);
}

long nanonet::detail_::my_send( socketfd_t const fd , char const* p , long n ) {

  long ret ;
  do { ret = ::send( fd , p , n , 0 ) ; }
  while( nanonet::detail_::EINTR_repeat( ret ) ) ;

  return ret ;

}

long nanonet::detail_::my_sendto( 
    socketfd_t const fd , 
    const sockaddr* const a, const socklen_t len, 
    char const* p , long n ) 
{

  long ret ;
  do { ret = ::sendto( fd , p , n , 0 , a , len ) ; }
  while( nanonet::detail_::EINTR_repeat( ret ) ) ;

  return ret ;

}
