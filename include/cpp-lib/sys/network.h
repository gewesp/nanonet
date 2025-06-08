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
// Component: NETWORK
//
// Supports:
// * Datagram (UDP) and stream (TCP) abstractions
// * IPv4 and IPv6
// * IOstreams abstractions TCP
// * Name resolution (DNS)
//
// Notes:
// * In order to send from client datagram socket, the address family
//   must match the destination address.
//
// TODO:
// * Client and server constructors or factory functions for UDP.
// * The API is still in flux and currently not in line with N1925.
// * In addition to timeout parameters, add buffer size (SO_SNDBUF, SO_RCVBUF).
//   NOTE: Linux maximum values seem to default to ca. 200k, can be increased:
//   # View current values
//   sysctl net.core.rmem_max
//   sysctl net.core.wmem_max
// 
//   # Set new values
//   sysctl -w net.core.rmem_max=new_value
//   sysctl -w net.core.wmem_max=new_value
// * Define an options struct instead of individual parameters?
//

#ifndef CPP_LIB_NETWORK_H
#define CPP_LIB_NETWORK_H

#include <vector>
#include <istream>
#include <ostream>
#include <streambuf>
#include <stdexcept>
#include <exception>
#include <limits>
#include <iostream>
#include <iterator>

#include <cstdlib>

#include <boost/noncopyable.hpp>
#include <boost/lexical_cast.hpp>

#include "cpp-lib/util.h"
#include "cpp-lib/sys/net-util.h"

#include "cpp-lib/detail/network.h"
#include "cpp-lib/detail/net-address.h"
#include "cpp-lib/detail/socket.h"
#include "cpp-lib/detail/socket_lowlevel.h"

namespace cpl {

namespace util {

namespace network {

struct acceptor   ;
struct connection ;


////////////////////////////////////////////////////////////////////////
// Proposed standard interface.
////////////////////////////////////////////////////////////////////////

typedef cpl::detail_::address< SOCK_STREAM >   stream_address ;
typedef cpl::detail_::address< SOCK_DGRAM  > datagram_address ;

typedef std::vector<   stream_address >   stream_address_list ;
typedef std::vector< datagram_address > datagram_address_list ;


// 
// Resolver functions.
//
// The following combinations exist:
// * resolve_{stream|datagram}(service, address_family_hint);
// -> Resolves to an address on any local interface, intended to bind.
// -> Typically used for server sockets
//
// * resolve_{stream|datagram}(hostname, service, address_family_hint);
// -> Resolves to an address on the given remote host, intended to send or 
//    connect.
// -> Typically used for client sockets
//
// address_family_hint: Prefer ipv4 or ipv6 if specified, defaults to
//     ip_unspec meaning any suitable protocol.
//
// Return value:
// * A list of at least one suitable addresses.
// * Throws if the hostname/service/address_family_hint combination cannot 
//   be resolved.
//

template< typename ... ARG >
inline cpl::util::network::stream_address_list 
resolve_stream( ARG&& ... arg ) {
  return cpl::detail_::resolve< SOCK_STREAM >(
      std::forward< ARG >( arg ) ... 
  ) ;
}

template< typename ... ARG >
inline cpl::util::network::datagram_address_list 
resolve_datagram( ARG&& ... arg ) {
  return cpl::detail_::resolve< SOCK_DGRAM >(
      std::forward< ARG >( arg ) ... 
  ) ;
}

////////////////////////////////////////////////////////////////////////
// Datagram communications.
//
// TODO: Implement smart decision on which address from resolver to
// use.
// Datagram sockets are thread safe except that uncoordinated calls to 
// connect() from different threads can yield unpredictable results.
////////////////////////////////////////////////////////////////////////

struct datagram_socket {

  // Types

  typedef datagram_address      address_type      ;
  typedef datagram_address_list address_list_type ;

  typedef unsigned long size_type ;

  //////////////////////////////////////////////////////////////////////// 
  // Constructors
  //////////////////////////////////////////////////////////////////////// 
  // Note: Prefer the 'named' constructors (static members returning
  // a datagram_socket) below.  The other constructors may become
  // deprecated at some point.

  //////////////////////////////////////////////////////////////////////// 
  // Moveable, but not copyable
  //////////////////////////////////////////////////////////////////////// 
  datagram_socket           (datagram_socket&&) = default;
  datagram_socket& operator=(datagram_socket&&) = default;

  datagram_socket           (datagram_socket const&) = delete;
  datagram_socket& operator=(datagram_socket const&) = delete;

  // Creates an unbound ('client') IPv4 or IPv6 socket.
  // User code should use e.g. datagram_socket sock(ipv4);
  // Use this constructor for 'client' sockets, i.e. sockets intended
  // to send data and possibly process replies.  The port (service) will
  // be chosen by the operating system.
  datagram_socket( address_family_type ) ;

  // Creates a bound ('server') socket.
  // Binds to local service ls.
  // Use this constructor for server sockets receiving data on a given port.
  datagram_socket( address_family_type , std::string const& ls ) ;

  // Creates a bound ('server') socket.
  // Binds to local name and service
  // Use (any_ipv4()/"0.0.0.0", ls) for IPv4
  // Use (any_ipv6()/"::", ls) for IPv6
  // Use this constructor for server sockets receiving data on a given port.
  datagram_socket( std::string const& ln, std::string const& ls ) ;

  // Creates a bound ('server') socket.
  // Binds to the first suitable of the given local addresses
  datagram_socket( address_list_type const& la ) ;

  //////////////////////////////////////////////////////////////////////// 
  // Named constructors
  //////////////////////////////////////////////////////////////////////// 
  // Returns a socket connected to the given hostname and service,
  // preferring the given address family if specified other than
  // ip_unspec.
  static datagram_socket connected(
      std::string const& name , std::string const& service ,
      address_family_type family = ip_unspec ) ;

  // Returns a socket connected to the given address
  static datagram_socket connected(
      address_type const& destination ) ;

  // Returns a socket bound to the local service ls with the given family
  inline static datagram_socket bound( 
      address_family_type const family , std::string const& ls ) {
    return datagram_socket( family , ls ) ;
  }

  // Creates a bound ('server') socket.
  // Binds to local name and service
  // Use (any_ipv4()/"0.0.0.0", ls) for IPv4
  // Use (any_ipv6()/"::", ls) for IPv6
  // Use this constructor for server sockets receiving data on a given port.
  inline static datagram_socket bound(
      std::string const& ln, std::string const& ls ) {
    return datagram_socket( ln , ls ) ;
  }

  //////////////////////////////////////////////////////////////////////// 
  // Operation
  //////////////////////////////////////////////////////////////////////// 
  // Constant returned by receive() functions in case of a timeout.
  static size_type timeout() 
  { return std::numeric_limits< size_type >::max() ; }

  // Default maximum size value for receive() functions.
  static size_type default_size() { return 65536 ; }

  // Connect to the given name/service, use IPv4 or IPv6 according
  // to own socket family.
  void connect( std::string const& name , std::string const& service ) ;

  // Connect to peer address.  Packets sent by send() will
  // be sent to this address.
  void connect( address_type const& destination ) {
    cpl::detail_::my_connect( s.fd() , destination.sockaddr_pointer() , destination.length() ) ;
  }

  // Receive a packet with timeout t [s].
  // t: Timeout.  If >= 0, waits for a maximum of t seconds (even zero).
  // If t < 0, blocks.
  // Returns: timeout() on timeout, or the number of characters written.
  // n: Maximum size to receive.
  template< typename for_it >
  size_type receive( 
    for_it const& begin , 
    double const& t = -1 ,
    size_type n = default_size()
  ) {
    return receive_internal( nullptr , begin , t , n );
  }

  // Like receive() above, but fills the source address
  template< typename for_it >
  size_type receive( 
    address_type& source ,
    for_it const& begin , 
    double const& t = -1 ,
    size_type n = default_size()
  ) {
    return receive_internal( &source , begin , t , n );
  }


  // Send overloads.
  // Connection refused error is ignored.

  // Sends to address given in connect().
  // Throws if connect() has not been called.
  template< typename for_it >
  void send( for_it const& begin, for_it const& end ) ;

  // Sends to given destination.
  template< typename for_it >
  void send( 
    for_it       const& begin       ,
    for_it       const& end         ,
    address_type const& destination
  ) ;
 
  // Sends to given node/service.
  template< typename for_it >
  void send( 
    for_it      const& begin   ,
    for_it      const& end     ,
    std::string const& node    ,
    std::string const& service
  ) ;


  // Observers

  // Removed.  Please use the new receive() overload with a source
  // return parameter instead.
  // address_type const& source() const;

  // Returns the local address.
  // Only valid for bound sockets, that is created with a local address
  // or after at least one send() call.
  // If the socket is not bound, may throw or return an all-zero address.
  address_type local() const { 
    return cpl::detail_::my_getsockname< SOCK_DGRAM >( fd() ) ;
  }

  // Returns the remote (peer) address.
  // Only valid for connected sockets, that is after at least one call
  // to connect().
  // If the socket is not connected, may throw or return an all-zero address.
  address_type peer () const {
    return cpl::detail_::my_getpeername< SOCK_DGRAM >( fd() ) ;
  }

private:
  template< typename for_it >
  size_type receive_internal(
    address_type* source ,
    for_it const& begin , 
    double const& t ,
    size_type n 
  ) ;

  // Enables broadcasting
  void initialize();

  cpl::detail_::datagram_socket_reader_writer s ;
  cpl::detail_::socketfd_t fd() const { return s.fd() ; }

} ;


////////////////////////////////////////////////////////////////////////
// Stream communications.
////////////////////////////////////////////////////////////////////////

//
// istreambuf<> and ostreambuf<> specializations for stream (TCP)
// sockets.
//

typedef cpl::util::istreambuf< cpl::detail_::stream_socket_reader_writer > 
instreambuf ;

typedef cpl::util::ostreambuf< cpl::detail_::stream_socket_reader_writer > 
onstreambuf ;

} // namespace network

namespace file {

template<> struct buffer_maker_traits< ::cpl::util::network::connection > {
  typedef cpl::util::network::instreambuf istreambuf_type ;
  typedef cpl::util::network::onstreambuf ostreambuf_type ;
} ;

} // namespace file

namespace network {

// A TCP connection endpoint, for incoming or outgoing connections.
// See tcp-test.cpp for examples.  Use instream/onstream for sending and
// receiving data.
struct connection : cpl::util::file::buffer_maker< connection > {

  // Moveable, but not copyable
  connection           (connection&&) = default;
  connection& operator=(connection&&) = default;

  connection           (connection const&) = delete;
  connection& operator=(connection const&) = delete;
  
  typedef stream_address      address_type      ;
  typedef stream_address_list address_list_type ;

  //////////////////////////////////////////////////////////////////////// 
  // Outgoing connections
  //////////////////////////////////////////////////////////////////////// 

  // If local_addresses is nonempty, connects to one of the remote addresses in ra 
  // using one of the local addresses la.
  // If local_addresses is empty, uses an unbound socket suitable for connection to
  // the first address in remote_addresses.
  // If a server is only listening on IPv6 or IPv4, this will still 
  // be able to connect, provided that both addresses are in remote_addresses.
  /// Throws a timeout_exception after timeout [s] if timeout >= 0.
  connection( 
      address_list_type const& remote_addresses                      ,
      address_list_type const& local_addresses = address_list_type() ,
      double timeout = -1.0
  ) ;

  // Connects to the given remote name/service (hostname/port).
  // Throws a timeout_exception after timeout [s] if timeout >= 0.
  connection( 
      std::string const& name    ,
      std::string const& service ,
      double timeout = -1.0
  ) ;

  //////////////////////////////////////////////////////////////////////// 
  // Incoming connections
  //////////////////////////////////////////////////////////////////////// 
  /// Waits for an incoming connection on the given acceptor.
  /// Throws a timeout_exception after timeout [s] if timeout >= 0.
  connection( acceptor& , double timeout = -1.0 ) ;

  //////////////////////////////////////////////////////////////////////// 
  // Parametrization
  //////////////////////////////////////////////////////////////////////// 

  // Enables/disables the TCP_NODELAY option.  If set, sends out data as 
  // soon as possible, otherwise waits a bit (RFC1122/`Nagle algorithm').
  void no_delay( bool = true ) ;

  // Sets send/receive timeout, parameter must be >= 0.
  // Default: Wait indefinitely
  void    send_timeout( double ) ;
  void receive_timeout( double ) ;
  // Sets both the send and receive timeouts to the same value >= 0.
  // Default: Wait indefinitely
  void timeout        ( double ) ;

  // Returns the other endpoint's address
  address_type const& peer () const { return  peer_ ; }
  // Returns the local address
  address_type const& local() const { return local_ ; }

  // Returns the internal socket object
  // TODO: This should be private.
  std::shared_ptr<cpl::detail_::stream_socket_reader_writer> socket() 
  { return s ; }

  // Returns the low-level socket descriptor
  cpl::detail_::socketfd_t fd() { return s->fd() ; }

  // Implementation of buffer_maker<> interface
  instreambuf make_istreambuf() { return instreambuf( socket() ) ; }
  onstreambuf make_ostreambuf() { return onstreambuf( socket() ) ; }

private:

  void shutdown_read () { cpl::detail_::socket_shutdown_read ( fd() ); }
  void shutdown_write() { cpl::detail_::socket_shutdown_write( fd() ); }

  // *************** IMPORTANT ******************
  // This *must* be a shared pointer to share with the streambuf
  // classes.
  // Therefore we can:
  // * Have both instream and onstream on one connection
  // * Continue to use the streams after connection goes out of scope.
  std::shared_ptr< cpl::detail_::stream_socket_reader_writer > s ;

  // Tries to find a matching pair from remote address and local
  // address and returns an accordingly initialized socket.
  std::shared_ptr< cpl::detail_::stream_socket_reader_writer > initialize(
    address_list_type const& ra , 
    address_list_type const& la ,
    double timeout
  ) ;

  address_type local_ ;
  address_type  peer_ ;

} ;


// Class to listen on a local port and establish incoming TCP connections
// (via the connection(acceptor&) constructor).  See tcp-test.cpp for
// examples.
struct acceptor {

  // Moveable, but not copyable
  acceptor           (acceptor&&) = default;
  acceptor& operator=(acceptor&&) = default;

  acceptor           (acceptor const&) = delete;
  acceptor& operator=(acceptor const&) = delete;

  typedef stream_address      address_type      ;
  typedef stream_address_list address_list_type ;

  // Listens on the given local service (port). Tries IPv4, IPv6
  // if IPv4 isn't available.
  // Backlog: Maximum queue size for incoming connections.
  acceptor( std::string const& ls , int backlog = 0 ) ;

  // Listens on the given local address and service (port), IPv4 or IPv6
  // Use (any_ipv4()/"0.0.0.0", ls) for IPv4
  // Use (any_ipv6()/"::", ls) for IPv6
  acceptor( std::string const& ln , std::string const& ls , int backlog = 0 ) ;

  // Listens on first suitable local address from the given list.
  acceptor( address_list_type const& la , int backlog = 0 ) ;

  // Returns the address we're listening on
  address_type const& local() const { return local_ ; }

  // Returns the low-level file descriptor
  cpl::detail_::socketfd_t fd() const { return s.fd() ; }

private:

  // Socket read/write interface, but the acceptor will only use 
  // accept().
  cpl::detail_::stream_socket_reader_writer s ;

  address_type local_ ;

} ;



//
// std::istream and std::ostream classes for stream (TCP) sockets.
// The constructor takes a connection object and attaches the data
// stream to it.  instream/onstream acquires shared ownership of the 
// connection so that the connection can optionally go out of scope 
// before the attached stream(s).
//
// At most one instream and one onstream may be created per connection
// (TODO: enforce that!).
//
// A pair of instream/onstream attached to the same connection may be
// used in separate threads.
//
// The instream/onstream destructors shut down the connection for
// reading/writing, respectively.  In order to signal the shutdown
// to other endpoint (typically causing it to shut down as well),
// the onstream should be destructed first.  This can be achieved
// by appropriate scoping, see the compact telnet example in tcp-test.cpp.
//

typedef cpl::util::file::owning_istream<instreambuf> instream ;
typedef cpl::util::file::owning_ostream<onstreambuf> onstream ;

inline instream make_instream( connection& c )
{ return instream( c ) ; }

inline onstream make_onstream( connection& c )
{ return onstream( c ) ; }

#if 0
// Peer functions.
// TODO: These are dangerous because after shutdown getpeername()
// no longer works.  Cache the info in the onstream object or
// hold a reference to the connection.
stream_address peer( instream const& ) ;
stream_address peer( onstream const& ) ;
stream_address local( instream const& ) ;
stream_address local( onstream const& ) ;
#endif

#if 0
// Removed because we removed virtual inheritance from the streambuf
// classes.  We need to clearly understand virtual inheritance
// and its influence on constructors before this can be
// reintroduced.  
// Mixing input and output is often a bad idea anyway.
struct nstream : std::iostream {

  nstream( connection& c ) 
  : std::iostream( 0 ) ,
    buf          ( c )
  { rdbuf( &buf ) ; }

private:

  nstreambuf buf ;

} ;
#endif


} // namespace network

} // namespace util

} // namespace cpl


////////////////////////////////////////////////////////////////////////
// Template definitions.
////////////////////////////////////////////////////////////////////////

// TODO: This should be done with timeout socket options, not select?!
// At the very least, use poll or ppoll, not select!
template< typename for_it >
cpl::util::network::datagram_socket::size_type 
cpl::util::network::datagram_socket::receive_internal( 
  address_type* const source_ret ,
  for_it const& begin ,
  double const& t , 
  size_type const max
) {

  if( t >= 0 ) {

    // Watch fd to see if it has input

    fd_set rfds ;
    FD_ZERO(        &rfds ) ;
    FD_SET ( fd() , &rfds ) ;

    // Do not wait.

    ::timeval tv = cpl::detail_::to_timeval( t ) ;

    int const nfds = fd() + 1 ;     // Size of array containing sockets 
                                    // up to and including the one we set.

    int err ;
    do 
    { err = ::select( nfds , &rfds , 0 , 0 , &tv ) ; } 
    while( cpl::detail_::EINTR_repeat( err ) ) ;  
    // Some platforms including Linux update timeout.

    if( err < 0 ) { cpl::detail_::throw_socket_error( "select" ) ; }

    if( err == 0 ) { return timeout() ; }

  }

  std::vector< char > buffer( max ) ;

  long err ;
  do {
    address_type source ;

    err = 
      ::recvfrom(
         fd()           ,
         &buffer[ 0 ]   ,
         buffer.size()  ,
         0              ,
         source.sockaddr_pointer() ,
         source. socklen_pointer()
      ) ;

    if( nullptr != source_ret ) {
      *source_ret = source ;
    }

  } while( cpl::detail_::EINTR_repeat( err ) ) ;

  if( err < 0 )
  { cpl::detail_::throw_socket_error( "recvfrom" ) ; }

  assert( static_cast< size_type >( err ) <= buffer.size() ) ;

  std::copy( buffer.begin() , buffer.begin() + err , begin ) ;
  return err ;

}


// TODO: Factor out common code from send(begin, end) and 
// send(begin, end, dest).
template< typename for_it >
void cpl::util::network::datagram_socket::send(
  for_it const& begin ,
  for_it const& end
) {
 
  std::vector< char > const buffer( begin , end ) ;

  long const result = 
      cpl::detail_::my_send( fd() , &buffer[ 0 ] , buffer.size() ) ;

  if( result < 0 ) {
    if( errno == ECONNREFUSED ) { return ; }
    cpl::detail_::throw_socket_error( "send" ) ;
  }

  always_assert( result == static_cast< long >( buffer.size() ) ) ;

}

template< typename for_it >
void cpl::util::network::datagram_socket::send(
  for_it const& begin ,
  for_it const& end   ,
  address_type const& d
) {

  // It appears we cannot send from an IPv4 socket to IPv6 or vice versa
  // (tested MacOS X).  Hence, test for this condition and throw
  // in case somebody tries.
  if( d.family_detail_() != local().family_detail_() ) {
    throw std::runtime_error( "datagram send: address family mismatch" ) ;
  }

  std::vector< char > const buffer( begin , end ) ;

  const long result = cpl::detail_::my_sendto(
      fd() , d.sockaddr_pointer() , d.length() , &buffer[ 0 ] , buffer.size() ) ;

  if( result < 0 ) {
    if( errno == ECONNREFUSED ) { return ; }
    cpl::detail_::throw_socket_error( "send to " + d.host() + ":" + d.port() ) ; 
  }

  always_assert( result == static_cast< long >( buffer.size() ) ) ;

}
 
template< typename for_it >
void cpl::util::network::datagram_socket::send( 
  for_it      const& begin   ,
  for_it      const& end     ,
  std::string const& node    , 
  std::string const& service
) {

  address_list_type const& ra = resolve_datagram( node , service ) ;

  for( auto const& i : ra ) {

    if( i.family_detail_() == local().family_detail_() ) {
      send( begin , end , i ) ; 
      return ;
    }

  }

  throw std::runtime_error( "datagram send: no matching address family" ) ;

}

#endif // CPP_LIB_NETWORK_H
