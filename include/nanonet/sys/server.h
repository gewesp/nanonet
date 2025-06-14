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
// Component: SYSUTIL
//
// ASCII line-based TCP server framework, suitable e.g. for telnet,
// HTTP etc.
//
// TODO:
// * More general, allow monitoring a server.  This could be done
//   by a use count in the acceptor (and/or shared pointer; even a
//   list of peer addresses is conceivable) and a monitoring thread
//   which logs parameters once in a while.  The monitoring thread
//   could then also terminate the server.
//   This requires support in the network data structures.
//

#ifndef NANONET_SYS_SERVER_H
#define NANONET_SYS_SERVER_H

#include "nanonet/sys/network.h"

#include <atomic>
#include <functional>
#include <iosfwd>
#include <optional>
#include <stdexcept>
#include <thread>


namespace nanonet {

namespace util {

/// Forward declarations
struct server_status;

/// A flag to control the running state of one or more 
/// background tasks.  This is in fact an atomic boolean 
/// that can only change state from true to false.
struct running_flag {
  bool running() const { return running_; }

  void shutdown() { running_ = false; }

private:
  std::atomic_bool running_ = true;
};

/// Server manager, returned from run_server()
struct server_manager {
  /// Constructs a no-op server manager, no logging
  server_manager() {}

  /// Constructs a no-op server manager, with optional logging
  server_manager(
      const std::string& name,
      std::ostream* const logstream) 
  : name_(name),
    logstream_(logstream)
  {}

  /// Non-copyable
  server_manager           (const server_manager&) = delete;
  server_manager& operator=(const server_manager&) = delete;

  /// Moveable
  server_manager           (server_manager&&) = default;
  server_manager& operator=(server_manager&&) = default;

  /// Constructs a server manager for the given thread
  server_manager(
      const std::string& name,
      std::ostream* const logstream, std::thread&& t)
  : name_(name),
    logstream_(logstream), 
    thread_(std::move(t))
  {}

  /// Waits until the server has no more connections and shut down.  
  /// This is a no-op for foreground servers.
  /// Optionally logs to the logstream supplied in the constructor, so make
  /// sure that this is still around!
  ~server_manager();

  /// @return The server name as given in the constructor
  const std::string& name() const { return name_; }

private:
  /// Server name
  std::string   name_ = "(anonymous)";
  std::ostream* logstream_ = nullptr;
  std::thread   thread_;
};


//
// A server is built around a handler function.
//
// Handler parameters are:
// * The input line
// * Input and output streams bound to the network connection
// * A stream for logging.  A new stream is created for 
//   for each incoming connection for use exclusively in the
//   thread handling the connection.
// * Current server status, read-only.  Status is not computed
//   for test servers (service "test:stdio").
//
// Handlers *must* be copyable.  Each thread serving a connection gets
// a copy of the handler.
//
// Handler return value:
//   The function must return false if the connection should be closed,
//   e.g. if the peer has issued a quit command.
//
// Handler exceptions:
//   Must be derived from std::exception.
//   The framework catches and logs all excptions escaping from the
//   handler.  In case of a shutdown_exception, the running flag of
//   the corresponding server is set to false with the intention of
//   shutting down the server.
//

typedef std::function<
  bool(std::string const& line,
       std::istream& ins,
       std::ostream& ons,
       std::ostream& log,
       const server_status&)> input_handler_type;


//
// This function is called at the beginning of a server to write a
// welcome message.
//

typedef std::function<
  void(std::ostream& ons, const server_status&)> os_writer;

//
// Connection parameters.
// bind_address    ... The local address to bind to, default: "0.0.0.0"
// service         ... Port, or "test:stdio" for test run on stdin/stdout
// server_name     ... Server name for syslog
// n_listen_retries ... Retry this many times to listen to incoming
//                      connections, typically to wait for the port to
//                      become available.  A value of -1 means to retry
//                      indefinitely.
// listen_retry_time ... Time to wait for listening port to become available
//                     after a failure to listn [s]
// log_connections ... Whether to log connections or not
//                     ("New connection", "Connection closing" log entries)
// max_line_length ... Maximum for input lines
// timeout         ... I/O timeout.  Connections time out if during this time
//                     nothing has been sent nor received [s].  Timeout causes
//                     EOF on any onstream/instream.
// accept_timeout  ... If no connection comes in during this time, the service
//                     cycles the accept loop, thus checking the running_flag [s].
// C_cps_{slow,medium,fast} ... Mix-in constants for the connection 
//                     rate estimators, must be between 0 and 1
// backlog         ... Max number of backlogged connections (see acceptor)
// background      ... Whether to listen for connections in background or not
// shutdown_wait_for_client_close ... Whether or not to wait for the client
//                     to close the connection before we close it on shutdown.
//                     Setting this parameter to true can help enforce a
//                     client-close-first policy, thus avoiding 'address
//                     already in use' errors on server restart.
//
// For test mode, timeouts, backlog and background are ignored.
//

struct server_parameters {
  server_parameters() {} 

  std::string bind_address = nanonet::util::network::any_ipv4();
  std::string service    = "test:stdio";
  std::string server_name = "nanonet/generic";
  bool   log_connections = true ;
  long   n_listen_retries = -1  ;
  double listen_retry_time = 1.0;
  long   max_line_length = 1000 ;
  double timeout         = 60.0 ;
  double accept_timeout  = 3.0  ;

  double C_cps_slow   = 0.002   ;
  double C_cps_medium = 0.01    ;
  double C_cps_fast   = 0.05    ;

  int    backlog         = 0    ;
  bool   background      = false;
  bool   shutdown_wait_for_client_close = true;
};

/// Server status, passed to each connection handler
struct server_status {
  server_status() {}

  server_status(const server_parameters& params)
  : name(params.server_name) {}

  /// Name of this server
  std::string name;

  /// Total number of connections so far
  std::atomic_llong connections_total = 0;

  /// Current number of connections
  std::atomic_long connections_current = 0;

  /// Connections per second estimate, long averaging
  /// See C_cps_{slow,medium,fast} above
  std::atomic<double> cps_estimate_slow   = 0.0;

  /// Connections per second estimate, medium averaging
  std::atomic<double> cps_estimate_medium = 0.0;

  /// Connections per second estimate, fast averaging
  std::atomic<double> cps_estimate_fast   = 0.0;
};

/// @return "(thread <threadid>)" for the current thread ID
std::string this_thread_id_paren();

//
// Starts an IPv4 server on port params.service with the given
// backlog.
//
// Logs start with params.server_name in syslog or on the given
// ostream (sl), if non-null.  The ostream *must* outlive the
// returned server_manager object since it is used in its destructor
// as well.  Scoping rules normally ensure this.
//
// If welcome is given, uses the function to write a message to its
// stream argument at the beginning of each connection.
//
// This version can only be used with params.background set to false.
// Once the function returns without an exception, it is guaranteed 
// that the server shutdown is complete, i.e. there are no more 
// connections active.  A shutdown can be requested by connection 
// handlers.
//
// Each connection runs a getline() loop calling handler.  The connection
// is closed as soon as the handler returns false or throws.
//
// If any connection handler throws a shutdown_exception, calls
// shutdown() on running and the server tries to shut down.
// A shutdown can also be requested 'externally' by calling shutdown()
// on running.
//
// THROWS: On errors listening to the port, e.g. privilege error
// or address already in use.  Exceptions in connection handlers
// are logged and the connection is terminated.
//
// CAUTION: getline() trims \n, but *not* \r, so there may be
// remaining whitespace at the end of the line.
//
// NOTE: You'll typically need to cast the welcome function to
// the os_writer type, i.e.
//   void welcome_func(std::ostream& os) { ... }
//   nanonet::util::run_server( ..., nanonet::util::os_writer{welcome_func}, ...);
//
// If params.background is true, the function returns immediately
// and the returned server_manager's destructor will wait until
// shutdown has completed, optionally logging to sl.  Assign
// the returned server_manager to a local variable in order to
// ensure correct behaviour.
//

[[nodiscard]] server_manager run_server(
    input_handler_type const& handler,
    running_flag& running,
    std::optional<os_writer> welcome = std::nullopt,
    server_parameters const& params = server_parameters(),
    std::ostream* sl = nullptr);

} // namespace util

} // namespace nanonet

#endif // NANONET_UTIL_SYS_H
