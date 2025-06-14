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

#ifndef NANONET_NET_UTIL_H
#define NANONET_NET_UTIL_H

#include <string>

namespace nanonet {
namespace util    {
namespace network {

/// Constants for IPv4 and IPv6.
/// Avoid overlap of enum with port numbers.
enum address_family_type { 
  ipv4      = 1000123, 
  ipv6      = 1000343, 
  ip_unspec = 1000999 
};

/// @return ipv4 for "ipv4", "ip4" etc.
///
/// Throws if description cannot be recognized.
/// allow_unspec: If true, allows 'any' or 'unspec' as description
/// and returns ip_unspec in that case.
address_family_type address_family( 
    std::string const& description ,
    bool allow_unspec = false);

/// Checks n and throw an exception if it is not a valid port number.
void check_port(long long);

/// @returns "0.0.0.0", the name for INADDR_ANY.  Use this to 
/// explicitly bind to an IPv4 address.
std::string any_ipv4();

/// @return "::", the name for INADDR6_ANY.  Use this to 
/// explicitly bind to an IPv6 address.
std::string any_ipv6();

} // namespace network
} // namespace util
} // namespace nanonet

#endif // NANONET_NET_UTIL_H
