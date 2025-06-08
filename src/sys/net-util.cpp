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

#include "cpp-lib/sys/net-util.h"

#include "cpp-lib/assert.h"


cpl::util::network::address_family_type
cpl::util::network::address_family(
    std::string const& desc , bool const allow_unspec ) {
  if        ( "ip4" == desc || "ipv4" == desc) {
    return cpl::util::network::ipv4;
  } else if ( "ip6" == desc || "ipv6" == desc ) {
    return cpl::util::network::ipv6;
  } else if ( "unspec" == desc || "any" == desc ) {
    if ( allow_unspec ) {
      return cpl::util::network::ip_unspec;
    } else {
      throw std::runtime_error(
          "need to specify address family ipv4 or ipv6" ) ;
    }
  } else {
    throw std::runtime_error( "unkown address family: " + desc ) ;
  }
}

void cpl::util::network::check_port(const long long n)
{
  if( n < 0 || n > 65535 )
  { cpl::util::throw_runtime_error( "TCP/UDP port number out of range of 0 to 65535" ) ; }
}


std::string cpl::util::network::any_ipv4()
{
  return "0.0.0.0" ;
}        
         
std::string cpl::util::network::any_ipv6() 
{
  return "::" ;
}        
