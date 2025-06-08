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

#include "cpp-lib/sys/network.h"

#include "cpp-lib/assert.h"
#include "cpp-lib/util.h"
#include "cpp-lib/detail/network.h"
#include "cpp-lib/detail/platform_net_impl.h"
#include "cpp-lib/detail/platform_wrappers.h"
#include "cpp-lib/detail/socket_lowlevel.h"
#include "cpp-lib/sys/syslogger.h"

#include "boost/lexical_cast.hpp"
#include "boost/cast.hpp"

#include <string>
#include <sstream>
#include <exception>
#include <stdexcept>

#include <cstring>
#include <cstdlib>
#include <cassert>
#include <cerrno>
#include <climits>

using namespace cpl::util::network ;
using namespace cpl::util          ;
using namespace cpl::detail_       ;

namespace {

template< int type >
void my_bind( socketfd_t const fd , address< type > const& a ) {
  cpl::detail_::my_bind(fd, a.sockaddr_pointer(), a.length());
}

template< int type >
void my_connect( socketfd_t const fd , address< type > const& a , const double timeout ) {
  cpl::detail_::my_connect(fd, a.sockaddr_pointer(), a.length(), timeout);
}

template< int type >
long my_sendto
( socketfd_t const fd , address< type > const& a , char const* p , long n ) {
  return cpl::detail_::my_sendto(fd, a.sockaddr_pointer(), a.length(), p, n);
}

// Returns a socket bound to the first matching address in la
// or throws.
template< int type > cpl::detail_::socket< type >
bound_socket( std::vector< address< type > > const& la ) {

  std::string err ;

  if( 0 == la.size() ) 
  { throw std::runtime_error( "must give at least one local address" ) ; }

  for( auto const& adr : la ) {

    try {

      cpl::detail_::socket< type > s( adr.family_detail_() ) ;
      ::my_bind( s.fd() , adr ) ;
      return s ;

    } catch( std::exception const& e ) { err = e.what() ; continue ; }

  }

  throw std::runtime_error( err ) ;

}

} // end anonymous namespace

int cpl::detail_::int_address_family( 
    cpl::util::network::address_family_type const af ) {
  switch( af ) {
    case cpl::util::network::ipv4 :
      return AF_INET ;
    case cpl::util::network::ipv6 :
      return AF_INET6 ;
    case cpl::util::network::ip_unspec :
      return AF_UNSPEC ;
    default :
      throw std::runtime_error( "unknown address family type" ) ;
  }
}

cpl::util::network::address_family_type 
cpl::detail_::from_int_address_family( int const afi ) {
  switch( afi ) {
    case AF_INET  :
      return cpl::util::network::ipv4 ;
    case AF_INET6 :
      return cpl::util::network::ipv6 ;
    case AF_UNSPEC :
      return cpl::util::network::ip_unspec ;
    default       :
      throw std::runtime_error( "unknown integer address family" ) ;
  }
}


socketfd_t cpl::detail_::socket_resource_traits::invalid()
{ return cpl::detail_::invalid_socket() ; }

bool cpl::detail_::socket_resource_traits::valid( socketfd_t const s )
{ return invalid() != s && s >= 0; }


//
// According to an FAQ, the proper sequence for closing a TCP connection
// gracefully is:
//   1. Finish sending data.
//   2. Call shutdown() with the how parameter set to 1 (SHUT_WR).
//   3. Loop on receive() until it returns 0.
//   4. Call closesocket().
//
// The connection/acceptor and {io}nstream classes strive to follow this
// pattern.
//

void cpl::detail_::socket_resource_traits::initialize
( socketfd_t const ) { }

void cpl::detail_::socket_resource_traits::dispose
( socketfd_t const s ) { 
  // The Linux man page close(2) has the following wisdome on close()
  // as of 6/2020:
  //
  //   The EINTR error is a somewhat special case.  
  //   Regarding the EINTR error, POSIX.1-2013 says:
  //   
  //   If close() is interrupted by a signal that is to be caught, it shall return 
  //   -1 with errno set to EINTR and the state of fildes is unspecified.
  //
  //   This permits the behavior that occurs on Linux and many other 
  //   implementations, where, as with other errors that may be reported by
  //   close(), the file descriptor is guaranteed to be closed. 
  // 
  // So, we'd rather not retry.  This has worked perfectly for years now.
  if( socketclose( s ) < 0 ) {
    cpl::util::log::log_oneoff(
        "SOCKET-DESTRUCTOR",
        cpl::util::log::prio::EMERG,
         "WTF: Close failed for socket descriptor " + std::to_string(s)
        + "; error: "
        + cpl::detail_::get_strerror_message(errno));
  }
}


////////////////////////////////////////////////////////////////////////
// Stream
////////////////////////////////////////////////////////////////////////

cpl::util::network::acceptor::acceptor
( address_list_type const& la , int const bl )
: s( bound_socket< SOCK_STREAM >( la ) ) ,
  local_( my_getsockname< SOCK_STREAM >( s.fd() ) )
{ cpl::detail_::my_listen( s.fd() , bl ) ; }

cpl::util::network::acceptor::acceptor
( std::string const& ls , int const bl )
: s( bound_socket< SOCK_STREAM >( resolve_stream( ls ) ) ) ,
  local_( my_getsockname< SOCK_STREAM >( s.fd() ) )
{ cpl::detail_::my_listen( s.fd() , bl ) ; }

cpl::util::network::acceptor::acceptor
( std::string const& ln , std::string const& ls , int const bl )
: s( bound_socket< SOCK_STREAM >( resolve_stream( ln , ls ) ) ) ,
  local_( my_getsockname< SOCK_STREAM >( s.fd() ) )
{ cpl::detail_::my_listen( s.fd() , bl ) ; }

std::shared_ptr<stream_socket_reader_writer>
cpl::util::network::connection::initialize( 
  address_list_type const& ra ,
  address_list_type const& la ,
  const double timeout
) {

  std::string err = "";

  // No local port given, use unbound socket.
  if( 0 == la.size() ) {

    for( auto const& remote : ra ) {

      try {

        auto const s = std::make_shared< stream_socket_reader_writer >( 
            remote.family_detail_() ) ;

        ::my_connect( s->fd() , remote , timeout ) ;
        return s;

      } catch( std::exception const& e ) { 
        if (not err.empty()) {
          err += "; ";
        }
        err += std::string("from (unbound local socket) to ") 
            +  to_string(remote)
            +  ": " 
            +  e.what();
        continue; 
      }

    }

  } else {
    // Local addresses given, find an address family match.

    for( auto const& local : la ) {

      for( auto const& remote : ra ) {

        if( local.family_detail_() != remote.family_detail_() ) 
        { continue ; }

        try {

          auto const s = 
              std::make_shared< stream_socket_reader_writer >( 
                  local.family_detail_() ) ;

          ::my_bind   ( s->fd() , local ) ;
          ::my_connect( s->fd() , remote , timeout ) ;
          return s;

        } catch( std::exception const& e ) { 
          if (not err.empty()) {
            err += "; ";
          }
          err += std::string("from ")
              +  to_string(local)
              +  " to " 
              +  to_string(remote)
              +  ": " 
              +  e.what();
          continue ; 
        }

      }

    }

  }

  if (err.empty()) {
    err = "Local/remote address families didn't match";
  }

  throw std::runtime_error("Failed to connect: " + err);
}
 

cpl::util::network::connection::connection
( std::string const& n , std::string const& serv , const double timeout )
: s( initialize( resolve_stream( n , serv ) , address_list_type() , timeout ) ) ,
  local_( my_getsockname< SOCK_STREAM >( fd() ) ) , 
  peer_ ( my_getpeername< SOCK_STREAM >( fd() ) )
{ }


cpl::util::network::connection::connection( 
  address_list_type const& ra ,
  address_list_type const& la ,
  const double timeout )
: s( initialize( ra , la , timeout ) ) ,
  local_( my_getsockname< SOCK_STREAM >( fd() ) ) , 
  peer_ ( my_getpeername< SOCK_STREAM >( fd() ) )
{ }

cpl::util::network::connection::connection
( acceptor& a , const double timeout )
: s( std::make_shared<stream_socket_reader_writer>(
         4711 , cpl::detail_::my_accept( a.fd() , timeout ) ) ) ,
  local_( my_getsockname< SOCK_STREAM >( fd() ) ) , 
  peer_ ( my_getpeername< SOCK_STREAM >( fd() ) )
{ }

void cpl::util::network::connection::no_delay( bool b )
{ bool_sockopt( fd() , TCP_NODELAY , b ) ; }

void cpl::util::network::connection::   send_timeout( const double t )
{ cpl::detail_::time_sockopt( fd() , SO_SNDTIMEO , t ) ; }

void cpl::util::network::connection::receive_timeout( const double t )
{ cpl::detail_::time_sockopt( fd() , SO_RCVTIMEO , t ) ; }

void cpl::util::network::connection::timeout( const double t ) {
     send_timeout( t ) ;
  receive_timeout( t ) ;
}

#if 0
// Dangerous ... not possible after shutdown().  Cf. comments in header.
cpl::util::network::stream_address
cpl::util::network::peer( cpl::util::network::onstream const& ons ) {
  return my_getpeername< SOCK_STREAM >( ons.buffer().reader_writer().fd() ) ;
}

cpl::util::network::stream_address
cpl::util::network::peer( cpl::util::network::instream const& ins ) {
  return my_getpeername< SOCK_STREAM >( ins.buffer().reader_writer().fd() ) ;
}
#endif

////////////////////////////////////////////////////////////////////////
// Datagram
////////////////////////////////////////////////////////////////////////

void cpl::util::network::datagram_socket::initialize() {
  bool_sockopt( s.fd() , SO_BROADCAST ) ;
  bool_sockopt( s.fd() , SO_REUSEADDR ) ;
}

// TODO: Use delegating constructors
cpl::util::network::datagram_socket::datagram_socket(
    cpl::util::network::address_family_type const af )
  : s( datagram_socket_reader_writer( int_address_family( af ) ) )
{ initialize() ; }

// TODO: A bit messy.  Another wrapper around my_getaddrinfo()?
cpl::util::network::datagram_socket::datagram_socket( 
    cpl::util::network::address_family_type const af ,
    std::string const& ls ) 
: s( bound_socket< SOCK_DGRAM >( 
      cpl::detail_::my_getaddrinfo< SOCK_DGRAM >( 
        NULL , /* name */
        ls.c_str() , /* service */
        int_address_family( af ) ) ) )
{ initialize() ; }

cpl::util::network::datagram_socket::datagram_socket( 
    std::string const& ln,
    std::string const& ls ) 
: s( bound_socket< SOCK_DGRAM >( resolve_datagram( ln , ls ) ) )
{ initialize() ; }

cpl::util::network::datagram_socket::datagram_socket(
  address_list_type const& la 
) : s( bound_socket< SOCK_DGRAM >( la ) )
{ initialize() ; }


cpl::util::network::datagram_socket
cpl::util::network::datagram_socket::connected(
    std::string const& name ,
    std::string const& service ,
    cpl::util::network::address_family_type const family ) {
  // resolve_datagram() either throws or returns at least one address.
  return cpl::util::network::datagram_socket::connected(
      cpl::util::network::resolve_datagram(
          name , service , family ).at( 0 ) ) ; 
}

cpl::util::network::datagram_socket
cpl::util::network::datagram_socket::connected(
    cpl::util::network::datagram_socket::address_type const& destination ) {
  cpl::util::network::datagram_socket ret( destination.family() ) ;
  ret.connect( destination ) ;
  return ret ;
}

void cpl::util::network::datagram_socket::connect(
    std::string const& name ,
    std::string const& service ) {
  address_list_type const& candidates = resolve_datagram( name , service ) ;

  // Look for protocol (IPv4/IPv6) match and connect.
  for ( auto const& adr : candidates ) {
    if ( local().family_detail_() == adr.family_detail_() ) {
      connect( adr ) ;
      return ;
    }
  }

  throw std::runtime_error( "datagram connect: address family mismatch" ) ;

}
