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
// Platform independent low-level socket functions.
//
// TODO: Implementations in .cpp, avoid inlining
//

#ifndef CPP_LIB_DETAIL_SOCKET_LOWLEVEL_H
#define CPP_LIB_DETAIL_SOCKET_LOWLEVEL_H

#include "cpp-lib/detail/platform_wrappers.h"


namespace nanonet {

namespace detail_ {

/// Throws an error with the given message and strerror_r(errno)
void throw_socket_error( std::string const& msg ) ;

/// @return SOL_SOCKET/SO_ERROR error status of the socket fd
int get_sockopt_error(socketfd_t fd);

/// Enables/disables the given binary socket option
void bool_sockopt( 
    socketfd_t fd , int option , bool enable = true );

/// Sets the given send or receive timeout (applies only to send()/recv()
/// calls, not to accept(), connect() etc...
/// option must be SO_SNDTIMEO or SO_RCVTIMEO.
void time_sockopt( socketfd_t fd , int option , double t ) ;

/// Enables/disables the given fcntl() option, e.g. O_NONBLOCK
void bool_fcntl_option(
    socketfd_t fd, int option, const std::string& error_msg, 
    bool enable);

/// socketsend() is a thin layer around send() to abstract away platform
/// differences.
///
/// SIGPIPE occurs on writing to a socket if the peer has already shut
/// it down.  This dangerous behavior seems to be mandated by POSIX.
///
/// The machanism for avoiding SIGPIPE is not specified in POSIX
/// and hence different across systems:
/// * Linux allows to avoid it on a per-send() basis (MSG_NOSIGNAL flag).  
/// * MacOS Darwin: Per-socket basis (SO_NOSIGPIPE).
/// TODO: Other systems.
int socketsend( socketfd_t fd , char const* data , std::size_t size ) ;

/// Platform dependent setup for stream sockets to guard against SIGPIPE
void setup_stream_socket( socketfd_t fd ) ;

} // detail_

} // cpl


#endif // CPP_LIB_DETAIL_SOCKET_LOWLEVEL_H
