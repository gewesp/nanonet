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

#ifndef CPP_LIB_DETAIL_NET_ADDRESS_H
#define CPP_LIB_DETAIL_NET_ADDRESS_H

#include "nanonet/assert.h"
#include "nanonet/sys/net-util.h"


#include "nanonet/detail/platform_wrappers.h"
#include "nanonet/detail/socket.h"
#include "nanonet/detail/socket_lowlevel.h"

#include <iostream>


namespace nanonet {

namespace detail_ {

// Returns AF_INET, AF_INET6 or AF_UNSPEC or throws in case of invalid value
int int_address_family( nanonet::util::network::address_family_type ) ;

// Returns ipv4, ipv6 or ip_unspec (on input AF_INET, AF_INET6, ...) or throws
// in case of invalid value
nanonet::util::network::address_family_type from_int_address_family( int ) ;


/// A safety wrapper around struct sockaddr_storage, templatized on
/// SOCK_DGRAM/SOCK_STREAM.
/// TODO: Support UNIX domain sockets!  .... How to implement host/port then?
template< int type >
struct address {

  // Initialize to ipv4:0.0.0.0:0
  address() : addrlen( maxlength() ) {
    std::memset( &addr , 0 , sizeof( addr ) ) ;
    sockaddr_pointer()->sa_family = AF_INET;
  }

  address( sockaddr_storage const& addr , socklen_t addrlen )
  : addr( addr ) , addrlen( addrlen ) {}

  // Numeric form.
  std::string const host() const ;
  std::string const port() const ;

  // These try reverse lookup and throw if it fails.
  std::string const host_name() const ;
  std::string const port_name() const ;

  std::string const fqdn() const ;

  bool dgram() const { return SOCK_DGRAM == type ; }

  // Returns the address family type (ipv4 or ipv6)
  nanonet::util::network::address_family_type family() const {
    return nanonet::detail_::from_int_address_family( family_detail_() ) ;
  }
  socklen_t length() const { return addrlen ; }

  socklen_t maxlength() const { return sizeof( sockaddr_storage ) ; }
  void set_maxlength() { addrlen = maxlength() ; }

  ////////////////////////////////////////////////////////////////////////  
  // Pseudo-private:  Reserved for implementation use.  
  // Do not use in application code.
  ////////////////////////////////////////////////////////////////////////  

  // Accessors for the low-level stuff...
  // The pointers are always valid (they point to memory held by
  // the object)
  sockaddr* sockaddr_pointer() 
  { return reinterpret_cast< sockaddr* >( &addr ) ; }

  sockaddr const* sockaddr_pointer() const
  { return reinterpret_cast< sockaddr const* >( &addr ) ; }

  sockaddr_in  const& as_sockaddr_in () const
  { return *reinterpret_cast< sockaddr_in  const* >( &addr ) ; }

  sockaddr_in6 const& as_sockaddr_in6() const
  { return *reinterpret_cast< sockaddr_in6 const* >( &addr ) ; }

  socklen_t const* socklen_pointer() const { return &addrlen ; }
  socklen_t      * socklen_pointer()       { return &addrlen ; }

  int family_detail_() const { return sockaddr_pointer()->sa_family ; }
private:

  // From struct addrinfo:
  sockaddr_storage addr    ;
  socklen_t        addrlen ;

} ;


// TODO: Naive implementation, maybe take sin6_flowinfo etc. into account?
// http://linux.die.net/man/7/ipv6
template< int type >
bool operator!=( address< type > const& a1 , address< type > const& a2 ) {
  if( a1.family_detail_() != a2.family_detail_() ) {
    return true;
  }

  if( AF_INET == a1.family_detail_() ) {
    return    a1.as_sockaddr_in().sin_port
           != a2.as_sockaddr_in().sin_port
        ||    a1.as_sockaddr_in().sin_addr.s_addr
           != a2.as_sockaddr_in().sin_addr.s_addr
    ;
  } else {
    return    a1.as_sockaddr_in6().sin6_port
           != a2.as_sockaddr_in6().sin6_port
        ||    !nanonet::util::mem_equal(a1.as_sockaddr_in6().sin6_addr,
                                    a2.as_sockaddr_in6().sin6_addr)
    ;
  }
}

template< int type >
bool operator==( address< type > const& a1 , address< type > const& a2 ) {
  return !(a1 != a2) ;
}

// String conversion, writing the numeric address and port.
// IPv6 addresses are surrounded by square brackets as per
// http://en.wikipedia.org/wiki/IPv6_address
template <int type>
std::string to_string(const address<type>& a) {
  std::string ret;
  if( AF_INET6 == a.family_detail_() ) {
    ret += '[';
  }
  ret += a.host() ;
  if( AF_INET6 == a.family_detail_() ) {
    ret += ']';
  }
  ret += ":" + a.port() ; 

  return ret;
}

// Output operator, using to_string()
template< int type >
std::ostream& operator<<( std::ostream& os , address< type > const& a ) {
  os << to_string(a);
  return os;
}

// Resolve implementations, see resolve_stream(), resolve_datagram()
template< int type >
std::vector< address< type > >
my_getaddrinfo( char const* n , char const* s , 
                int family_hint = AF_UNSPEC ) ;

template< int type > std::vector< nanonet::detail_::address< type > >
inline resolve( 
    std::string const& n , std::string const& s ,
    nanonet::util::network::address_family_type const hint = 
        nanonet::util::network::ip_unspec ) {
  return nanonet::detail_::my_getaddrinfo< type >( 
      n.c_str() , s.c_str() , nanonet::detail_::int_address_family( hint ) ) ;
}

template< int type > std::vector< nanonet::detail_::address< type > >
inline resolve(
    std::string const& s ,
    nanonet::util::network::address_family_type const hint =
        nanonet::util::network::ip_unspec ) {
  return nanonet::detail_::my_getaddrinfo< type >( 
      nullptr , s.c_str() , nanonet::detail_::int_address_family( hint ) ) ;
}

template< int type >
address< type > my_getsockname( socketfd_t const fd ) {

  // API information (from man getsockname):
  // getsockname() returns the current address to which the socket sockfd is
  // bound, in the buffer pointed to by addr.  The addrlen  argument  should
  // be initialized to indicate the amount of space (in bytes) pointed to by
  // addr.  On return it contains the actual size of the socket address.
  address< type > a ;
  always_assert( *a.socklen_pointer() == a.maxlength() ) ;
  int const err = 
    ::getsockname( fd , a.sockaddr_pointer() , a.socklen_pointer() ) ;

  if( err < 0 )
  { throw_socket_error( "getsockname" ) ; }

  return a ;

}

template< int type >
address< type > my_getpeername( socketfd_t const fd ) {

  address< type > a ;
  int const err = 
    ::getpeername( fd , a.sockaddr_pointer() , a.socklen_pointer() ) ;

  if( err < 0 )
  { throw_socket_error( "getpeername" ) ; }

  return a ;

}

template< int type >
std::string const
my_getnameinfo(
  address< type >  const& a ,
  bool             const node    ,  // node or service?
  bool             const numeric ,  // numeric or name?
  bool             const fqdn       // fqdn (for node)?
) ;


} // detail_

} // cpl


////////////////////////////////////////////////////////////////////////
// Template definitions
////////////////////////////////////////////////////////////////////////

inline const char* notnull( const char* const s ) {
  return s ? s : "(null)";
}

template< int type >
std::vector< nanonet::detail_::address< type > >
nanonet::detail_::my_getaddrinfo(
  char const* const n ,
  char const* const s ,
  int const family_hint
) {

  // std::cout << "Resolving " << notnull(n) << ":" << notnull(s) << std::endl;

  always_assert( SOCK_DGRAM == type || SOCK_STREAM == type ) ;
  always_assert( n || s ) ;
  int const flags = n ? 0 : AI_PASSIVE ;

  addrinfo* res = 0 ;
  addrinfo  hints   ;

  // Make sure everything's cleared...
  std::memset(&hints, 0, sizeof(hints));

  hints.ai_flags      = flags       ;
  hints.ai_family     = family_hint ;
  hints.ai_socktype   = type        ;

  // Be extra clear, cf. http://en.wikipedia.org/wiki/Getaddrinfo :
  // In some implementations (like the Unix version for Mac OS), the 
  // hints->ai_protocol will override the hints->ai_socktype value while
  // in others it is the opposite, so both need to be defined with
  // equivalent values for the code to be cross platform.
  hints.ai_protocol   = SOCK_DGRAM == type ? IPPROTO_UDP : IPPROTO_TCP ;

  hints.ai_addrlen    = 0 ;
  hints.ai_addr       = 0 ;
  hints.ai_canonname  = 0 ;
  hints.ai_next       = 0 ;

  int const err = getaddrinfo( n , s , &hints , &res ) ;

  if( err ) {

    always_assert( !res ) ;
    
    if( n && s ) {

      throw std::runtime_error( 
          std::string( "can't resolve " )
        + n
        + ":" 
        + s
        + ": " 
        + ::gai_strerror( err ) 
      ) ;

    } else {
      
      throw std::runtime_error( 
          std::string( "can't resolve " )
        + s
        + ": " 
        + ::gai_strerror( err ) 
      ) ;

    }

  }

  std::vector< nanonet::detail_::address< type > > ret ;

  for( addrinfo const* p = res ; p ; p = p->ai_next ) { 

    always_assert( type == p->ai_socktype ) ;
    ret.push_back( 
      address< type >( 
        *reinterpret_cast< sockaddr_storage* >( p->ai_addr    ) ,
                                                p->ai_addrlen
        )
      )
    ;

  }

  always_assert( res ) ;
  ::freeaddrinfo( res ) ;

  always_assert( ret.size() >= 1 ) ;

#if 0
  for (unsigned i = 0; i < ret.size(); ++i ) {
    std::cout << "Found: " << ret[i] << std::endl;
  }
#endif

  return ret ;

}


template< int type >
std::string const 
nanonet::detail_::my_getnameinfo( 
  nanonet::detail_::address< type > const& a ,
  bool             const node    ,
  bool             const numeric ,
  bool             const fqdn
) {

  char n[ NI_MAXHOST ] ;
  char s[ NI_MAXSERV ] ;

  int const flags = 
      ( numeric   ? NI_NUMERICHOST | NI_NUMERICSERV : NI_NAMEREQD )
    | ( fqdn      ? 0                               : NI_NOFQDN   )
    | ( a.dgram() ? NI_DGRAM                        : 0           )
  ;

  int const err =
    node ?
      getnameinfo
      ( a.sockaddr_pointer() , a.length() , n , NI_MAXHOST , 0 , 0 , flags )
    : 
      getnameinfo
      ( a.sockaddr_pointer() , a.length() , 0 , 0 , s , NI_MAXSERV , flags )
  ;

  if( err ) {

    const std::string t = node ? "node" : "service" ;
    throw std::runtime_error( 
        "can't get name associated with " + t + ": " + gai_strerror( err ) 
    ) ;

  }

  return node ? n : s ;

}


template< int type >
std::string const
nanonet::detail_::address< type >::fqdn() const {

  nanonet::detail_::check_family( family_detail_() ) ;

  return nanonet::detail_::my_getnameinfo< type >( *this , true , false , true ) ;

}


template< int type >
std::string const
nanonet::detail_::address< type >::host_name() const {

  nanonet::detail_::check_family( family_detail_() ) ;

  return nanonet::detail_::my_getnameinfo< type >( *this , true , false , false ) ;

}

template< int type >
std::string const
nanonet::detail_::address< type >::port_name() const {

  nanonet::detail_::check_family( family_detail_() ) ;

  return nanonet::detail_::my_getnameinfo< type >( *this , false , false , false ) ;

}

template< int type >
std::string const
nanonet::detail_::address< type >::host() const {

  nanonet::detail_::check_family( family_detail_() ) ;

  return nanonet::detail_::my_getnameinfo< type >( *this , true , true , false ) ;

}

template< int type >
std::string const
nanonet::detail_::address< type >::port() const {

  return nanonet::detail_::my_getnameinfo< type >( *this , false , true , false ) ;

}

#endif // CPP_LIB_DETAIL_NET_ADDRESS_H
