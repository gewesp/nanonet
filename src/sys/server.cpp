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

#include "cpp-lib/sys/server.h"

#include "cpp-lib/math-util.h"
#include "cpp-lib/util.h"

#include "cpp-lib/sys/util.h"
#include "cpp-lib/sys/syslogger.h"


#include <exception>
#include <iostream>
#include <functional>
#include <stdexcept>
#include <string>
#include <thread>

using namespace nanonet::util          ;
using namespace nanonet::util::log     ;
using namespace nanonet::util::network ;

std::string nanonet::util::this_thread_id_paren() {
  // TODO: Replace by std::format(); requires full C++23 support
  std::ostringstream oss;
  oss << "(thread " << std::this_thread::get_id() << ")";
  return oss.str();
}

namespace {

using count_sentry = nanonet::util::increment_sentry<std::atomic_long>;

// A thread for each incoming connection
struct connection_thread {
  // Somehow these don't get automatically created with
  // reference wrappers?!
  connection_thread           (connection_thread&&) = default;
  connection_thread& operator=(connection_thread&&) = default;

  connection_thread(
      std::unique_ptr<nanonet::util::network::connection> c,
      count_sentry sentry_in,
      server_parameters const& params,
      input_handler_type const& handler,
      std::optional<os_writer> const welcome,
      std::reference_wrapper<nanonet::util::running_flag> running_in,
      std::reference_wrapper<nanonet::util::server_status> status_in)
    : c{std::move(c)},
      sentry(std::move(sentry_in)),
      params{params},
      handler{handler},
      welcome{welcome},
      running(running_in),
      status(status_in)
  {}

  // Connection count must be handled in operator(), or the move
  // constructors must be made aware of it.
  void operator()();

private:

  std::unique_ptr<connection> c;
  count_sentry sentry;
  server_parameters params;
  input_handler_type handler;
  std::optional<os_writer> welcome;
  std::reference_wrapper<nanonet::util::running_flag> running;
  std::reference_wrapper<nanonet::util::server_status> status;
};


/// Maintain/update the connection rates per server
struct connection_rates {
  /// Initializes estimators with given parameters
  connection_rates(const server_parameters& params);

  /// Updates estimates when a connection comes in at time now [s]
  void update(double now);

  /// Updates cps values in provided status
  void update_status(server_status&) const;

private:
  nanonet::math::rate_estimator re_slow  ;
  nanonet::math::rate_estimator re_medium;
  nanonet::math::rate_estimator re_fast  ;
};


void handle_connection(
    std::ostream& sl, 
    const server_parameters& params,
    const nanonet::util::server_status& status,
    std::optional<os_writer> const welcome,
    input_handler_type const& handler,
    std::istream& is,
    std::ostream& os,
    nanonet::util::running_flag& running) {

  if (welcome) {
    (welcome.value())(os, status);
  }

  std::string line;
  while (running.running() and nanonet::util::getline(is, line, params.max_line_length)) {
    if (not handler(line, is, os, sl, status)) {
      break;
    }
  }
}

void connection_thread::operator()() { 
  if (not running.get().running()) {
    return;
  }
  syslogger sl{params.server_name + " connection " + this_thread_id_paren()};

  // Threads must not throw!  Try to be extra sure and wrap
  // instream/onstream constructors as well.
  try {
  instream is(*c);
  onstream os(*c);
  try {

  if (params.log_connections) {
    sl << prio::NOTICE << "New connection from " << c->peer() 
       << "; currently " << status.get().connections_current
       << "/total " << status.get().connections_total
       << " connection(s)"
       << std::endl;
  }

  ::handle_connection(sl, params, status, welcome, handler, is, os, running);

  // Timeout, EOF or error.  There's currently no good way to distinguish
  // these cases.
  if (params.log_connections) {
    sl << prio::NOTICE << "Connection closing: " << c->peer() << std::endl;
  }

  // Inner try
  } catch (nanonet::util::shutdown_exception const& e) {
    running.get().shutdown();
    sl << prio::NOTICE << "Shutdown requested in connection from: " 
       << c->peer() << std::endl;

    if (params.shutdown_wait_for_client_close) {
      sl << prio::NOTICE << "Waiting for client to close the connection..."
         << std::endl;
      char c = 0;
      while (is.get(c)) {}
      sl << prio::NOTICE << "Client closed connection, ready for shutdown"
         << std::endl;
    }
  } catch (std::exception const& e) {
    sl << prio::ERR << "In connection from " << c->peer()
       << ": " 
       << e.what() << std::endl;
  }

  // Outer try
  } catch (std::exception const& e) {
    sl << prio::CRIT 
       << "WTF: Communication couldn't be set up in connection from "
       << c->peer()
       << ": " 
       << e.what() << std::endl;
  }
}

struct server_thread {
  server_thread           (server_thread&&) = default;
  server_thread& operator=(server_thread&&) = default;
  server_thread(
      acceptor&& a_in,
      input_handler_type const& handler_in,
      std::optional<os_writer> const welcome_in,
      const server_parameters& params_in,
      std::reference_wrapper<nanonet::util::running_flag> running_in)
  : a(std::move(a_in)),
    handler(handler_in),
    welcome(welcome_in),
    params(params_in),
    running(running_in)
  {}

  void operator()();

private:
  acceptor a;
  input_handler_type handler;
  std::optional<os_writer> welcome;
  server_parameters params;
  std::reference_wrapper<nanonet::util::running_flag> running;
};

void log_params(
    std::ostream& sl, 
    server_parameters const& params, 
    bool const production) {

  sl << prio::NOTICE << "Starting service: " << params.server_name << std::endl;
  sl << prio::NOTICE << "Mode: " 
     << (production ? "Production" : "Test") << std::endl;
  sl << prio::NOTICE << "Maximum backlog: " << params.backlog << std::endl;
  sl << prio::NOTICE << "Connection timeout [s]: " 
                     << params.timeout << std::endl;
  sl << prio::NOTICE << "Maximum line length: " 
                     << params.max_line_length << std::endl;
  sl << prio::NOTICE << "Running in background: "
                     << params.background
                     << std::endl;
}

void server_thread::operator()() {
  syslogger sl{params.server_name + " listen " + this_thread_id_paren()};

  log_params(sl, params, true);

  sl << prio::NOTICE << "Listening for incoming connections on " 
                     << a.local() 
                     << std::endl;

  // Status for this server
  nanonet::util::server_status status(params);

  // Connection rate estimators
  ::connection_rates re(params);

  // Loop: Handle incoming connections on acceptor a
  // This loop terminates only when running becomes false.
  // Any exceptions cause a retry after 1s.
  while (running.get().running()) {

  try {
    std::unique_ptr<connection> c(new connection(a, params.accept_timeout));
    // Set connection timeout and pass it to the handler thread
    c->timeout(params.timeout);
    
    re.update(nanonet::util::utc());
    re.update_status(status);
    ++status.connections_total;
    connection_thread ct(std::move(c), count_sentry(status.connections_current), params, handler, welcome, running, status);
    // sl << prio::NOTICE 
    //    << "Starting connection thread..."
    //    << std::endl;

    std::thread t(std::move(ct));

    // sl << prio::NOTICE 
    //    << "Detaching connection thread..."
    //    << std::endl;

    // We detach, *but* keep a count of them.  Connections should either time out
    // or exit if running becomes false.
    // FIXME(maybe): We should keep track of the threads here and properly
    // join them.  However, the solution with count_sentry seems to be
    // sound as well.
    t.detach();
  } catch (const nanonet::util::timeout_exception&) {
    // Just continue...
    // sl << prio::NOTICE
    //    << "No connection came in during " 
    //    << std::to_string(params.accept_timeout) << "s, "
    //    << "checking running flag and continuing..."
    //    << std::endl;
  } catch (std::exception const& e) {
    nanonet::util::log::log_error(
        sl, "Failed to handle incoming connection", e.what());
    // Avoid busy loop in case of persistent errors
    nanonet::util::sleep(1);
  }

  } // while (running)

  sl << prio::NOTICE 
     << "Service loop terminated and shutdown initiated..."
     << std::endl;
 
  for (long n = 0; ; ++n) {
    const long count = status.connections_current;
    if (0 == count) {
      sl << prio::NOTICE 
         << "Service shutdown complete"
         << std::endl;
      break;
    }

    if (0 == n % 10) {
      sl << prio::NOTICE 
         << "Waiting for "
         << count 
         << " connection(s) to exit: Time [s]: " << n
         << std::endl;
    }

    nanonet::util::sleep(1);
  }
}

} // end anonymous namespace

  /// Initializes estimators with given parameters
::connection_rates::connection_rates(const server_parameters& params)
: re_slow  (params.C_cps_slow  ),
  re_medium(params.C_cps_medium),
  re_fast  (params.C_cps_fast  )
{}

void ::connection_rates::update(const double now) {
  re_slow  .update(now);
  re_medium.update(now);
  re_fast  .update(now);
}

void ::connection_rates::update_status(server_status& s) const {
  s.cps_estimate_slow   = re_slow  .estimate();
  s.cps_estimate_medium = re_medium.estimate();
  s.cps_estimate_fast   = re_fast  .estimate();
}

nanonet::util::server_manager nanonet::util::run_server(
    input_handler_type const& handler,
    nanonet::util::running_flag& running,
    std::optional<os_writer> const welcome,
    server_parameters const& params,
    std::ostream* sl) {
  if ("test:stdio" == params.service) {
    if (not sl) {
      sl = &std::cout;
    }
    assert(sl);
    try {
      log_params(*sl, params, false);
      nanonet::util::server_status status(params);
      ::handle_connection(
          std::cout, params, status, welcome, handler,
          std::cin, std::cout, running);
    } catch (nanonet::util::shutdown_exception const& e) {
      nanonet::util::log::log_error(
          *sl, 
            "Shutdown requested in synchronous test mode service " 
          + params.server_name, e.what());
    } catch (std::exception const& e) {
      nanonet::util::log::log_error(
          *sl, 
            "Aborting synchronous test mode service " 
          + params.server_name, e.what());
    }
  } else {
    std::unique_ptr<syslogger> sl_created;
    if (not sl) {
      sl_created = std::make_unique<syslogger>(
          params.server_name + " accept " + this_thread_id_paren());
      sl = sl_created.get();
    }
    always_assert(sl);
    long listen_retries = 0;

    while (true) {
      always_assert(sl);
      if (not running.running()) {
        *sl << prio::NOTICE
            << "Shutdown requested before we could bind to socket"
            << std::endl;
        break;
      }
      try {
        acceptor acc(params.bind_address, params.service, params.backlog);
        server_thread st(std::move(acc), handler, welcome, params, running);

        if (params.background) {
          *sl << prio::NOTICE 
              << "Starting service in background..." 
              << std::endl;
          std::thread t(std::move(st));
          return nanonet::util::server_manager(params.server_name, sl, std::move(t));
        } else {
          *sl << prio::NOTICE 
              << "Starting service in foreground and looping "
                 "to accept connections until shutdown..." 
              << std::endl;
          st();
        }
        break;
      } catch (std::exception const& e) {
        nanonet::util::log::log_error(
            *sl, "Failed to accept connections on " + params.service
                + " (retrying in " + std::to_string(params.listen_retry_time)
                + "s)",
                e.what());

        if (   params.n_listen_retries < 0 
            or ++listen_retries <= params.n_listen_retries) {
          nanonet::util::sleep(params.listen_retry_time);
          continue;
        } else {
          *sl << prio::ERR << "Maximum number of retries ("
             << params.n_listen_retries
             << ") reached, giving up" << std::endl;
          throw;
        }
      }
    }
  }

  // Returns a server_manager whose destructor will finish
  // instantly.
  return nanonet::util::server_manager(params.server_name, sl);
}

nanonet::util::server_manager::~server_manager() {
  if (thread_.joinable()) {
    if (logstream_) {
      *logstream_ << prio::NOTICE
                  << "Waiting for service shutdown confirmation: "
                  << name()
                  << std::endl;
    }
    thread_.join();
    if (logstream_) {
      *logstream_ << prio::NOTICE
                  << "Service shutdown confirmed: "
                  << name()
                  << std::endl;
    }
  } else {
    // No-op in case no thread is active.
    // CAUTION: This means that the thread_ object also serves
    // as a flag as to whether the object has been moved away
    // from
  }
}
