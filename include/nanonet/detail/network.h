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

#ifndef NANONET_DETAIL_NETWORK_H
#define NANONET_DETAIL_NETWORK_H


#include "nanonet/detail/platform_wrappers.h"


namespace nanonet {

namespace detail_ {

void my_listen( socketfd_t const fd , int const backlog );
void my_bind( socketfd_t const fd , const sockaddr* const a, const socklen_t len);

socketfd_t my_accept( socketfd_t const fd );

/// Returns a connected socket, or throws timeout_exception if timeout >= 0
/// and no connection comes in within timeout [s].
socketfd_t my_accept(const socketfd_t fd, double timeout);

/// Connects fd to the given address
void my_connect(socketfd_t fd , const sockaddr* a, socklen_t len);

/// Connects to the given address, or throws timeout_exception if timeout >= 0
/// and no connection can be establiehed in within timeout [s].
void my_connect(socketfd_t fd , const sockaddr* a, socklen_t len, double timeout);

long my_send(socketfd_t const fd , char const* p , long n );
long my_sendto(socketfd_t const fd , const sockaddr* a, socklen_t len, char const* p, long n);

} // detail_

} // nanonet

#endif // NANONET_DETAIL_NETWORK_H
