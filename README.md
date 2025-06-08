# Intuitive and idiomatic UDP and TCP sockets in modern C++


## Server example: Hello world on a TCP socket

```
#include "nanonet/tcp.h"

#include <format>
#include <iostream>

using namespace nn = nanonet;

void run_hello_server(std::string const& port) {
  nn::acceptor a(port);
  std::cerr << "Hello world server version 1.01 listening on "
            << a.local()
            << std::endl;

  while (true) {
    nn::connection c(a);
    std::log << std::format("Handling connection from {} ...", c.peer())
             << std::endl;

    nn::onstream(c) << std::format("Hello {}, pleased to meet you!, c.peer())
                    << std::endl;
  }
}
```

## Compile the library, tests and examples

Nanonet requires a modern C++ compiler such as `g++` or `clang++`.

```
./scripts/build.sh
cd build
./tcp-test wget http://www.github.com/
```


## IPv4 and IPv6 support 

IP addresses can be constructed from strings, so no IPv4 or IPv6 specific code
is required.

IPv6 notation is as follows:
```
[2001:db8:85a3:8d3:1319:8a2e:370:7348]:443
```

## More information

The implementation is roughly based on the
[Networking Proposal for TR2](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2005/n1925.pdf)

Differences to the TR2 proposal:
- nanonet uses uses the namespace `nanonet`, not `std::tr2`.
- nanonet doesn't implement the `network_error` exceptions, but throws
  `std::runtime_error` instead.
- I/O multiplexing (`iowait()`) is not implemented.


## More client and server examples

Please see [tcp-test.cpp](testing/tcp-test.cpp) and [udp-test.cpp](testing/udp-test.cpp).

### TCP

* To get the current time from NIST daytime servers:
  
  `tcp-test daytime`

* To download a web site:

  `tcp-test wget http://www.boost.org/ > foo.html`
  
  `less foo.html`

* To start a multi-threaded string reversal server listening on port 4711:

  `tcp-test reverse 4711`

  Connect to the server:

  `telnet localhost 4711`, or:
  `tcp-test telnet localhost 4711`

* A simple telnet client:

  `tcp-test telnet localhost 4711`
  `tcp-test telnet your.telnet.server 80`

* Run a hello world server and receive a greeting using netcat (nc):

  `tcp-test hello 12345`
  `nc localhost 12345`


### UDP

To receive data on port 4711 using IPv6:

  `udp-test receive ip6 4711`

To send data to port 4711:

  `udp-test send ip6 ::1 4711`

A simple pong server that listens for incoming packets and sends a reply:
  `udp-test pong ip6 4711`

In another window, you can send a message and receive the reply:
  `udp-test ping ip6 ::1 4711 "Hello world"`

Use ip4 and 127.0.0.1 instead of ::1 and 4711 if your system doesn't 
support IPv6.


### DNS

* To resolve a datagram address:

  `udp-test resolve google.com 4711`

  Notice that multiple node/service pairs may be returned.


# Bugs

See [TODO](TODO.md). 


# References 

* [Networking Proposal for TR2](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2005/n1925.pdf)
