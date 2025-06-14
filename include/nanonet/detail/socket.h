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

#ifndef NANONET_DETAIL_SOCKET_H
#define NANONET_DETAIL_SOCKET_H

#include "nanonet/assert.h"
#include "nanonet/sys/net-util.h"

#include "nanonet/detail/platform_wrappers.h"
#include "nanonet/detail/socket_lowlevel.h"

namespace nanonet {

namespace detail_ {

// Returns AF_INET, AF_INET6 or AF_UNSPEC or throws in case of invalid value
int int_address_family( nanonet::util::network::address_family_type ) ;

// Returns ipv4, ipv6 or ip_unspec (on input AF_INET, AF_INET6, ...) or throws
// in case of invalid value
nanonet::util::network::address_family_type from_int_address_family( int ) ;

struct socket_resource_traits {

  static socketfd_t invalid() ;
  static bool valid  ( socketfd_t const s ) ;
  static void initialize( socketfd_t const s ) ;
  static void dispose( socketfd_t const s ) ;
  
} ;


typedef 
nanonet::util::auto_resource< socketfd_t , socket_resource_traits > 
auto_socket_resource ;

/// Checks socket type (SOCK_DGRAM or SOCK_STREAM) and returns it
int check_socktype( int const type ) ;

/// Checks address family type (AF_INET6 or AF_INET) and returns it
int check_family( int const family ) ;

// A socket template with some intelligence, e.g. it checks
// its parameters.  Suitable for stream and datagram, specialized
// below.  Holds an auto_resource for the socket descriptor.
// Implements the 'reader_writer' abstraction.
template< int type > struct socket {

  // Just to be sure, clarify desired semantics, in accordance
  // with the auto_resource member.
  socket& operator=( const socket&  ) = delete;
  socket           ( const socket&  ) = delete;

  socket           ( socket&&      ) = default ;
  socket& operator=( socket&&      ) = default ;

  socketfd_t fd() const { return fd_.get() ; }

  bool valid() const { return fd_.valid() ; }

  void setup() const {
    
    if( !fd_.valid() )
    { throw_socket_error( "socket" ) ; }

    if( SOCK_STREAM == type ) 
    { setup_stream_socket( fd() ) ; }

  }


  // Constructor overload with a dummy parameter to distinguish
  // from constructor with family.  Used if the file descriptor
  // is already known (e.g., accept).
  socket( int const /* ignored */, socketfd_t const fd ) : fd_( fd )
  { setup() ; }

  socket( int const family )
  : fd_( ::socket( check_family( family ) , check_socktype( type ) , 0 ) ) 
  { setup() ; }

  long read ( char      * buf , long n ) ;
  long write( char const* buf , long n ) ;

  // No-ops for DGRAM sockets
  void shutdown_read () 
  { if ( SOCK_STREAM == type ) { socket_shutdown_read ( fd() ) ; } }
  void shutdown_write()
  { if ( SOCK_STREAM == type ) { socket_shutdown_write( fd() ) ; } }

private:

  // This wrapper around the handle takes care of move semantics.
  auto_socket_resource fd_ ;

} ;

typedef socket< SOCK_DGRAM  > datagram_socket_reader_writer ;
typedef socket< SOCK_STREAM >   stream_socket_reader_writer ;

} // detail_

} // nanonet


////////////////////////////////////////////////////////////////////////
// Template definitions
////////////////////////////////////////////////////////////////////////

template< int type >
long nanonet::detail_::socket< type >::read
( char      * const buf , long const n ) {

  long ret ;
  do { ret = ::recv( fd() , buf , n , 0 ) ; }
  while( EINTR_repeat( ret ) ) ;

  assert( -1 <= ret      ) ;
  assert(       ret <= n ) ;

  return ret ;

}

template< int type >
long nanonet::detail_::socket< type >::write
( char const* const buf , long const n ) {

  long ret ;

  // TODO: Handle partial send
  do { ret = nanonet::detail_::socketsend( fd() , buf , n ) ; }
  while( EINTR_repeat( ret ) ) ;

  if( ret < 0 || n != ret )
  { return -1 ; }

  assert( n == ret ) ;
  return ret ;

}

#endif // NANONET_DETAIL_SOCKET_H
