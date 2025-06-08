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

#include "cpp-lib/detail/socket.h"

#include "cpp-lib/assert.h"
#include "cpp-lib/detail/platform_wrappers.h"


int cpl::detail_::check_socktype( int const type )
{ always_assert( SOCK_DGRAM == type || SOCK_STREAM == type ) ; return type ; }

int cpl::detail_::check_family( int const family )
{ always_assert( AF_INET == family || AF_INET6 == family ) ; return family ; }
