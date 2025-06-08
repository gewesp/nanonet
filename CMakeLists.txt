# TODOs
# - C++20
# - No Boost dependency
# - Simplify / modernize this cmake file

cmake_minimum_required(VERSION 3.10)

message("-- Cpp-lib cmake file")
message("-- Project source directory: ${PROJECT_SOURCE_DIR}")

# This is a C++17 project
set(CMAKE_CXX_STANDARD 17)
set(MY_WARNING_FLAGS -Wall -Wextra -Werror)

project(cpp-lib VERSION 0.88.2)

# Some of our own modules
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

# Required 3rd party stuff
# png++ is found using stuff in CMAKE_MODULE_PATH
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

# The serial library is only implemented using a Windows API
# TODO: Append that to cpp-lib sources as well
if (WINDOWS)
  list(APPEND TESTS serial-test)
endif()

############################################################################

set(public-headers
    "include/cpp-lib/array_ops.h"
    "include/cpp-lib/assert.h"
    "include/cpp-lib/bg-typedefs.h"
    "include/cpp-lib/cgi.h"
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
    "src/cgi.cpp"
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
  ${EIGEN3_INCLUDE_DIRS}
  ${png++_INCLUDE_DIRS}/png++
)

# for library export definitions.
include(GenerateExportHeader)
generate_export_header(cpp-lib)

# install section.

# variables used in the install section.
set(config_install_dir "lib/cmake")
set(include_install_dir "include")
set(generated_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(version_config "${generated_dir}/${PROJECT_NAME}ConfigVersion.cmake")
set(project_config "${generated_dir}/${PROJECT_NAME}Config.cmake")
set(targets_export_name "${PROJECT_NAME}Targets")

# not using namespaces yet.
set(namespace "${PROJECT_NAME}::")

# include tools for cmake packagins.
include(CMakePackageConfigHelpers)

# configure project versionning.
write_basic_package_version_file(
    "${version_config}" COMPATIBILITY SameMajorVersion
)

# configure project to be included using cmake.
configure_package_config_file(
    "cmake/Config.cmake.in"
    "${project_config}"
    INSTALL_DESTINATION "${config_install_dir}"
)

# export targets
install(
    TARGETS cpp-lib
    EXPORT "${targets_export_name}"
    LIBRARY DESTINATION "lib"
    ARCHIVE DESTINATION "lib"
    RUNTIME DESTINATION "bin"
    INCLUDES DESTINATION "${include_install_dir}"
)

# install headers.
install(
    DIRECTORY "include/cpp-lib"
    DESTINATION "${include_install_dir}"
    FILES_MATCHING PATTERN "*.h"
    )

# library export.
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/cpp-lib_export.h"
    DESTINATION "${include_install_dir}"
    )

# config and version files.
install(
    FILES "${project_config}" "${version_config}"
    DESTINATION "${config_install_dir}"
    )

# target files.
install(
    EXPORT "${targets_export_name}"
    DESTINATION "${config_install_dir}"
    )
