# nanonet TODOs


## Before anything else: Project setup / maintenance

* Make sure examples work exactly as specified.
* Have uniform directories, i.e. all shell scripts in scripts, only source code in src, include, testing etc.
  (mostly done)
* Set up clang-format
* Set up clang-tidy
* Get rid of boost: As of 6/2025, we still use predef.h and some trivial string
  functions (e.g. trim()). (almost done)
* Think of reasonable UTF8 support
  * Could just templatize the buffer implementations on signed vs. unsigned char type,
    with implicit default conversion between the two?


## Features

* Think of how to make it header only
* Use C++20 features


## Networking TODOs

- Implement performance preferences?

  See Java implementation:
    http://java.sun.com/j2se/1.5.0/docs/api/allclasses-frame.html

- Avoid/ignore SIGPIPE on socket write on Windows and other systems
  (it is implemented on Linux and Darwin/FreeBSD).  Unfortunately,
  there doesn't seem to be a portable (POSIX) way to do that.

- Optimize UDP implementation:  Avoid buffer copies in case of
  contiguous ranges.

- Check defaults for address reuse and broadcasting in all constructors.

- `shutdown()`:  Remove unnecessary shutdowns (e.g. if other side already
  has shut down).  Try to do this without a stateful socket
reader/writer class, if at all possible (thread safety!).

- Implement `iowait()` (multiplexing), or a kind of signaller/interruptor 
  object.

- Implement the exception hierarchy from N1925.

- More example code.
