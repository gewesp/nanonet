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

#include "cpp-lib/error.h"
#include "cpp-lib/http.h"
#include "cpp-lib/registry.h"
#include "cpp-lib/util.h"
#include "cpp-lib/sys/network.h"
#include "cpp-lib/sys/server.h"
#include "cpp-lib/sys/syslogger.h"
#include "cpp-lib/sys/util.h"

#include <iostream>
#include <iterator>
#include <string>
#include <sstream>
#include <exception>
#include <stdexcept>
#include <algorithm>
#include <optional>
#include <memory>
#include <random>
#include <thread>

#include <cassert>


// Daytime server, note that these may be replaced by UDP sometime soon,
// cf. http://tf.nist.gov/tf-cgi/servers.cgi
std::string const DAYTIME_SERVER = "time.nist.gov" ;
std::string const DAYTIME_PORT   = "daytime"       ;

// Default send/receive timeout
double const DEFAULT_TIMEOUT = 20;

using namespace cpl::util::network ;
using namespace cpl::util::log ;


void usage( std::string const& name ) {

  std::cerr << 
"usage: " << name << " <command>\n"
"Available commands:\n"
"daytime:             Connect to time.nist.gov at port 13 and report time.\n"
"cat      port:       Wait for connection and copy TCP stream to stdout.\n"
"reverse  port:       Start a reverse server, one thread per connection.\n"
"reverse_bg port:     Start a reverse server in background.\n"
"httpd    port:       Start an HTTP server on given port.\n"
"                     Serves .txt and .html files from current directory.\n"
"hello    port:       Start a hello world server, immediately closes connection.\n"
"source   port:       Starts a data source with two control commands.\n"
"connect  host port:  Connect, copy stdin into connection and then\n"
"                     connection to stdout.\n"
"tee      host ports...:  Copy stdin to all ports on given host.\n"
"telnet   host port [ timeout ]:  Similar to connect, but copy data\n"
"                     from connection to stdout as soon as it appears.\n"
"                     Optional: Connect timeout [s]\n"
"wget     URL:        Request URL using HTTP/1.0 and dump the content\n"
"                     (including HTTP headers!) to stdout.\n"
// "tiles    config:     Download map tiles as per config.\n"
  ;

}


void reverse_service_welcome(
    std::ostream& os, const cpl::util::server_status& status) {
  os << "500 Welcome to " << status.name << "\n"
        "500 Thread ID: " << std::this_thread::get_id() << "\n"
        "500 Current number of connections: " << status.connections_current << "\n"
        "501 Max idle time: " << DEFAULT_TIMEOUT << "s\n"
        "501\n"
        "501 Please type the strings you would like to have reversed.\n"
        "501\n"
        "501 Type ``close'' to end the session.\n"
        "501 Type ``shutdown'' or ``sd'' to kill the server.\n"
        "501 Type ``status'' for server status.\n"
      << std::endl ;
}

bool reverse_service_handle_line(
    std::string const& s,
    std::istream& ins,
    std::ostream& ons,
    std::ostream& log,
    const cpl::util::server_status& status) {

  static_cast<void>(ins);
  static_cast<void>(log);

  std::istringstream iss(s);
  std::string ss;
  while (iss >> ss) {
  
    if( "close" == ss ) { 
      ons << "550 Goodbye!" << std::endl ; 
      return false;
    } else if( "status" == ss ) {
      ons << "101 server status:\n"
          << "101 server name: " << status.name << '\n'
          << "101 current number of connections: " << status.connections_current << '\n'
          << "101 total number of connections: " << status.connections_total << '\n'
          << "101 connections per second (slow estimate)  : " << status.cps_estimate_slow   << '\n'
          << "101 connections per second (medium estimate): " << status.cps_estimate_medium << '\n'
          << "101 connections per second (fast estimate)  : " << status.cps_estimate_fast
          << std::endl;
    } else if( "shutdown" == ss or "sd" == ss ) {
      ons << "201 shutdown requested\n"
          << "201 please close this connection, then the server will exit"
          << std::endl;
      throw cpl::util::shutdown_exception("Server shutdown requested: " + ss);
    }

    reverse( ss.begin() , ss.end() ) ; 
    ons << ss << std::endl ; 
  }

  return true;
}

/// Handles an HTTP GET request.
/// @param line The GET request, e.g. "GET /foobar HTTP/1.1"
/// @param is Stream which contains the rest of the request, will
/// be parsed until the first empty line.
/// @param os Stream for the response
/// @param log Logging
bool http_service_handle_line(
    std::string const& line,
    std::istream& is,
    std::ostream& os,
    std::ostream& log,
    const cpl::util::server_status& status) {
  const auto request = cpl::http::parse_get_request(line, is, &log);

  log << prio::NOTICE << "Handling GET request; "
                      << "Path: " << request.abs_path
                      << "; User agent: " << request.user_agent
                      << "; Total number of connections: " << status.connections_total
                      << std::endl;

  try {
    auto file = cpl::util::file::open_read("." + request.abs_path);
    // Try to read one character
    file.peek();
    if (file.bad() or file.fail()) {
      cpl::util::throw_error("Failed to open: " + request.abs_path);
    }

    cpl::http::write_http_header_200(
        os, cpl::http::content_type_from_file_name(request.abs_path));

    // TODO: Handle read errors on file.  Not clear how HTTP handles
    // an error *during* transmission...
    cpl::util::stream_copy(file, os);
  } catch (const std::exception& e) {
    log << prio::ERR << "GET request failed: " << e.what()
        << std::endl;
    cpl::http::write_http_header_404(os, e.what());
  }

  // Only one request per connection, tell framework to please close it.
  return false;
}

void print_connection( connection const& c ) {

  std::cerr << "Local address: " << c.local() << std::endl ;
  std::cerr << "Peer address: "  << c.peer () << std::endl ;

}


void tee( std::istream& is , 
          std::string const& host , const char* const* ports ) {
  std::vector< onstream > ons   ;

  while( *ports ) {
    connection c( host , *ports ) ;
    ons.push_back( make_onstream( c ) ) ;
    std::cout << "Connected to " << c.peer() << std::endl ;

    ++ports;
  }

  std::string l ;
  while( std::getline( is , l ) ) {
    for( auto& os : ons ) {
      os << l << std::endl ;
      if( !os ) {
        // TODO: Store connection info or peer address
        // in ons and report it here...
        throw std::runtime_error( "Write failed" ) ;
      }
    }
  }
}


void run_datasource( std::string const& port ) {
  
  acceptor a( port ) ;
  std::cerr << "Data source example version 0.02 listening on " 
            << a.local()
	    << std::endl ;

  while( 1 ) {

    connection c( a ) ;
    std::cerr << "Connection from: " << c.peer() << std::endl ;

    onstream os( c ) ;

    bool send_data = true;
    bool running   = true;
      
    auto const command_handler = [&c, &running, &send_data]() {
      instream is(c);
      std::string command;

      while (is >> command) {
        if        ("quit"      == command) {
          running = false;
          break;
        } else if ("send_data" == command) {
          is >> send_data;
        } else {
          // TODO: thread sync
          // os << "error: unknown command: " << command << std::endl;
        }
      }
    };

    std::thread command_handler_thread(command_handler);

    os << "200 Welcome to the data source.\n"
       << "200 Type send_data <0|1> to switch off/on sending.\n"
       << "200 Type quit to exit" << std::endl;

    while (os && running) {
      if (send_data) {
        os << "{ id: foobar, time: " 
           << std::setprecision(12) << cpl::util::utc() 
           << ", data: [1,2,3,4] }"
           << std::endl ;
      }

      cpl::util::sleep(2.0);
    }

    command_handler_thread.join();
    
  }

}

void run_hello_server( std::string const& port ) {
  
  acceptor a( port ) ;
  std::cerr << "Hello world server version 0.01 listening on " 
            << a.local()
	    << std::endl ;

  while( 1 ) {

    connection c( a ) ;
    std::cerr << "Saying hello to: " << c.peer() << std::endl ;

    onstream os( c ) ;
    
    os << "Hello " << c.peer() << "!  What a pleasure to meet you!\n" ;
    
  }

}

void run_reverse_server( std::ostream& sl, std::string const& port , const bool background ) {

  cpl::util::server_parameters p;
  p.service = port;
  p.n_listen_retries = 10;
  p.timeout = DEFAULT_TIMEOUT;
  p.server_name = "REVERSE-SERVER-0.92";
  p.log_connections = true;
  p.background = background;

  cpl::util::running_flag running;

  std::cout << "Starting reverse server; background = " << background
            << std::endl;

  const auto manager = cpl::util::run_server(
      reverse_service_handle_line,
      running,
      // Explicit cast necessary here because two steps are involved
      cpl::util::os_writer{reverse_service_welcome}, p, &sl);

  // If server runs in foreground, return from run_server() means
  // that shutdown is complete.
  if (not background) {
    std::cout << "Server shutdown completed; exiting"
              << std::endl;
    return;
  }

  // Otherwise, we don't know. 
  std::cout << "Please enter sd or shutdown server, x or exit to\n"
            << "test for clean shutdown and exit"
            << std::endl;

  std::string cmd;
  while (std::cin >> cmd) {
    if ("sd" == cmd or "shutdown" == cmd) {
      std::cout << "Shutdown command received" << std::endl;
      running.shutdown();
    } else if ("x" == cmd or "exit" == cmd) {
      // manager's destructor will log and join the server thread
      return;
    } else {
      std::cout << "I don't get you" << std::endl;
    }
  }
}

void run_http_server(std::ostream& sl, const std::string& port) {
  cpl::util::server_parameters p;
  p.service = port;
  p.server_name = cpl::http::default_server_identification();
  p.background = false;

  cpl::util::running_flag run;
  // Running in foreground, therefore we can ignore the 
  // returned server_manager (otherwise [[nodiscard]]).
  cpl::util::mark_unused(
      cpl::util::run_server(
          http_service_handle_line, run, std::nullopt, p, &sl));
}


// Copy is to os line by line, flushing after each line.
// This is not an exact byte per byte copy.
void line_copy( std::istream& is , std::ostream& os ) {
  std::string line ;
  while( std::getline( is , line ) && ( os << line << std::endl ) ) {}
}
 

void receive_data( connection& c , std::ostream& os = std::cout ) {

  std::cerr << "Data received:" << std::endl ;
  
  instream is( c ) ;
  cpl::util::stream_copy( is , os ) ;

  std::cerr << "EOF from server." << std::endl ;

}


void daytime() {

  connection c( DAYTIME_SERVER , DAYTIME_PORT ) ;
  print_connection( c ) ;

  instream is( c ) ;
  receive_data( c , std::cout ) ;

}


// Telnet client example, using separate send and receive threads.
void telnet( 
  std::istream& is        ,
  std::ostream& os        ,
  std::string const& host ,
  std::string const& port ,
  const double timeout
) {

  connection c( host , port , timeout ) ;
  instream ins( c ) ;

  std::thread receiver_thread( [&] { 
      line_copy( ins , os ) ; 
      os << "Connection closed by foreign host." << std::endl ;
  } ) ;

  {
    // By putting the onstream into a scope, we ensure that the connection
    // is properly shut down (this is done by the onstream destructor).
    // The server will notice the shutdown and shut down its side as well,
    // allowing the receiver thread to exit.
    onstream ons( c ) ;
    line_copy( is , ons ) ;
  }
  
  receiver_thread.join() ;
}

#if 0
// Download webmap tiles.  Needs port to a new configuration
// infrastructure
void tiles(std::ostream& sl, std::string const& config) {
  cpl::util::registry const reg(config);

  auto const tsp = cpl::map::tileset_parameters_from_registry(reg);
  tsp.validate();
  auto const    url_pattern = reg.check_string("url_pattern");
  auto const  local_pattern = reg.check_string("local_pattern");
  auto const tmpfile        = reg.check_string("tmpfile" );
  double const max_delay = reg.get_default("max_delay", 1.0);
  cpl::util::verify_bounds(max_delay, "max_delay", 0.0, 1e9);

  cpl::map::tile_mapper const tm(tsp);

  std::minstd_rand rng;
  std::uniform_real_distribution<> U(0.0, max_delay);

  cpl::util::file::mkdir(tsp.tile_directory, true);

  for (int zoom = tsp.maxzoom; zoom >= tsp.minzoom; --zoom) {
  std::string const dir1 = tsp.tile_directory + "/" + std::to_string(zoom);
  cpl::util::file::mkdir(dir1, true);

  auto const se_tile = tm.get_tile_coordinates(zoom, tsp.south_east);
  auto const nw_tile = tm.get_tile_coordinates(zoom, tsp.north_west);

  long const dx = se_tile.x - nw_tile.x;
  long const dy = se_tile.y - nw_tile.y;
  cpl::util::verify(dx >= 0, "assertion error");
  cpl::util::verify(dy >= 0, "assertion error");

  sl << prio::INFO << "Zoom level " << zoom 
     << ": Downloading " << (dx + 1) * (dy + 1)
     << " tile(s)" << std::endl;

  cpl::util::sleep(3.0);


  for (long x = nw_tile.x; x <= se_tile.x; ++x) {

  std::string const dir2 = dir1 + "/" + std::to_string(x);
  cpl::util::file::mkdir(dir2, true);

  for (long y = nw_tile.y; y <= se_tile.y; ++y) {
    char url  [1000] = "";
    char local[1000] = "";

    std::sprintf(url  ,   url_pattern.c_str(), zoom, x, y);
    // Local: Pattern only determines filename, directory structure is fixed
    std::sprintf(local, local_pattern.c_str(), y         );

    std::string const filename = dir2 + "/" + local;

    if (cpl::util::file::exists(filename)) {
      sl << prio::INFO << "File " << filename << " exists, skipping"
         << std::endl;
      continue;
    }

    auto localfile = cpl::util::file::open_write(filename);

    // Download and write file
    cpl::http::wget(sl, localfile, url);
    cpl::util::sleep(U(rng));
  }}

  }
}
#endif

int main( int argc , char const* const* const argv ) {
  cpl::util::log::syslogger sl;
  sl.set_echo_stream(&std::cout);
  sl.set_echo_clock([] { return 539917123; });

  try {

#if 0
  // Ignore broken pipe signal
  // This is not needed on Linux and Darwin because the signal is ignored
  // suppressed on a per-socket level.
  ::signal(SIGPIPE, SIG_IGN);
#endif

  if( argc <= 1 ) {

    usage( argv[ 0 ] ) ; 
    return 1 ;

  }
  
  std::string const command = argv[ 1 ] ;

  if( std::string( "cat" ) == command ) {

    if( 3 != argc ) { usage( argv[ 0 ] ) ; return 1 ; }
  
    acceptor a( argv[ 2 ] ) ;
    sl << prio::NOTICE << "Listening on: " << a.local() << std::endl ;

    connection c( a ) ;
    sl << prio::NOTICE << "Connection from: " << c.peer() << std::endl ;

    receive_data( c , std::cout ) ;

  } else if( "daytime" == command ) {

    if( 2 != argc ) { usage( argv[ 0 ] ) ; return 1 ; }

    daytime() ;

  } else if( "reverse" == command ) {
  
    if( 3 != argc ) { usage( argv[ 0 ] ) ; return 1 ; }
    run_reverse_server( sl , argv[ 2 ] , false ) ;

  } else if( "reverse_bg" == command ) {
  
    if( 3 != argc ) { usage( argv[ 0 ] ) ; return 1 ; }
    run_reverse_server( sl , argv[ 2 ] , true ) ;

  } else if( "httpd" == command ) {
  
    if( 3 != argc ) { usage( argv[ 0 ] ) ; return 1 ; }
    run_http_server( sl , argv[ 2 ] ) ;

  } else if( "hello" == command ) {
    
    if( 3 != argc ) { usage( argv[ 0 ] ) ; return 1 ; }
    run_hello_server( argv[ 2 ] ) ;

  } else if( "source" == command ) {
    
    if( 3 != argc ) { usage( argv[ 0 ] ) ; return 1 ; }
    run_datasource( argv[ 2 ] ) ;
    
  } else if( "tee" == command ) {

    if( argc < 4 ) { usage( argv[ 0 ] ) ; return 1 ; }
    tee( std::cin , argv[ 2 ] , &argv[ 3 ] ) ;

  } else if( "connect" == command ) {

    if( 4 != argc ) { usage( argv[ 0 ] ) ; return 1 ; }

    connection c( argv[ 2 ] , argv[ 3 ] ) ;
    print_connection( c ) ;

    sl << prio::NOTICE << "Enter request, terminate by Ctrl-D." << std::endl ;

    {
      onstream os( c ) ;
      cpl::util::stream_copy( std::cin , os ) ;
      // The onstream destructor shuts down write part of the connection.
    }

    sl << prio::NOTICE << "Request sent, waiting for reply." << std::endl ;
    
    receive_data( c , std::cout ) ;

  } else if( "telnet" == command ) { 

    double timeout = -1;
    if( argc < 4 or argc > 5 ) { usage( argv[ 0 ] ) ; return 1 ; }
    if( 5 == argc ) {
      timeout = std::stod( argv[ 4 ] );
    }

    telnet( std::cin , std::cout , argv[ 2 ] , argv[ 3 ] , timeout ) ;

  } else if( "wget" == command ) { 

    if( 3 != argc ) { usage( argv[ 0 ] ) ; return 1 ; }
    cpl::http::wget( sl , std::cout , argv[ 2 ] ) ;

#if 0
  } else if( "tiles" == command ) { 
    if( 3 != argc ) { usage( argv[ 0 ] ) ; return 1 ; }
    tiles( sl, argv[ 2 ] ) ;

#endif
  } else {
    
    usage( argv[ 0 ] ) ; 
    return 1 ;

  }

  } // end global try
  catch( std::exception const& e ) {
    sl << "Error: " << e.what() << std::endl ;
    sl << "Exiting..." << std::endl ;
    return 1 ;
  }

}
