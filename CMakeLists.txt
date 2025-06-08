# TODOs
# - C++20
# - No Boost dependency
# - Simplify / modernize this cmake file

cmake_minimum_required(VERSION 3.20)

message("-- Nanonet cmake file")
message("-- Project source directory: ${PROJECT_SOURCE_DIR}")

set(CMAKE_CXX_STANDARD 20)
set(MY_WARNING_FLAGS -Wall -Wextra -Werror)

project(nanonet VERSION 0.0.2)

# Some of our own modules
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

# Required 3rd party stuff
set(Boost_USE_STATIC_LIBS ON)
find_package(Boost 1.65.0 REQUIRED serialization)

message("-- Boost libraries version:  ${Boost_VERSION}"            )
message("-- Boost include directory:  ${Boost_INCLUDE_DIRS}"       )

# Test targets (see testing/)
set(TESTS 
    dispatch-test
    error-test
    # iowait
    # netutil
    # png-test
    # sched-test
    sched-thread-test
    tcp-test
    udp-test
    util-test)


############################################################################

set(public-headers
    "include/cpp-lib/assert.h"
    "include/cpp-lib/command_line.h"
    "include/cpp-lib/container-util.h"
    "include/cpp-lib/dispatch.h"
    "include/cpp-lib/error.h"
    "include/cpp-lib/exception.h"
    "include/cpp-lib/http.h"
    "include/cpp-lib/memory.h"
    "include/cpp-lib/registry.h"
    "include/cpp-lib/util.h"
    "include/cpp-lib/xdr.h"

    "include/cpp-lib/sys/file.h"
    "include/cpp-lib/sys/logger.h"
    "include/cpp-lib/sys/network.h"
    "include/cpp-lib/sys/realtime.h"
    "include/cpp-lib/sys/server.h"
    "include/cpp-lib/sys/syslogger.h"
    "include/cpp-lib/sys/util.h"
    )
set(private-headers
    "")
set(sources
    "src/assert.cpp"
    "src/command_line.cpp"
    "src/database.cpp"
    "src/dispatch.cpp"
    "src/error.cpp"
    "src/http.cpp"
    "src/math-util.cpp"
    "src/memory.cpp"
    "src/network.cpp"
    # "src/random.cpp"
    "src/registry.cpp"
    "src/util.cpp"

    "src/detail/network.cpp"
    "src/detail/socket.cpp"
    "src/detail/socket_lowlevel.cpp"

    "src/sys/file.cpp"
    "src/sys/logger.cpp"
    "src/sys/net-util.cpp"
    "src/sys/server.cpp"
    "src/sys/syslogger.cpp"
    "src/sys/util.cpp"
    )

if (UNIX)
  list(APPEND sources
    "src/posix/realtime.cpp"
    "src/posix/wrappers.cpp"
  )
endif()


############################################################################
# COMPILER CONFIGURATION
############################################################################
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DBOOST_DISABLE_ASSERTS")

############################################################################
# TARGET CONFIGURATION
############################################################################

add_library(cpp-lib
    ${public-headers}
    ${private-headers}
    ${sources}
    )

target_compile_options(cpp-lib PRIVATE ${MY_WARNING_FLAGS})


foreach(TEST ${TESTS})
  add_executable(${TEST} testing/${TEST}.cpp)
  target_link_libraries(${TEST} cpp-lib pthread)
  target_compile_options(${TEST} PRIVATE ${MY_WARNING_FLAGS})
  if (UNIX AND NOT APPLE)
    target_link_libraries(${TEST} rt)
  endif()
endforeach()

include_directories(SYSTEM
  ${PROJECT_SOURCE_DIR}/include
  ${Boost_INCLUDE_DIRS}
)
