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
// Component: UTIL
//

#ifndef NANONET_ASSERT_H
#define NANONET_ASSERT_H

#include <string>

#include "nanonet/exception.h"

namespace nanonet {

namespace util {

//
// Assertion failure.  If expr is false, call above die() with the message:
// ``Assertion failed: <expr> (<file>:<line>)''.
//

void assertion(
  const bool expr ,
  std::string const& expr_string ,
  std::string const& file ,
  const long line
) ;

//
// If expression is false, throw a std::runtime_error with the given message.
//

void verify( bool expression , std::string const& message ) ;


/// Throws a nanonet::util::timeout_exception with empty message.
[[noreturn]] void throw_timeout_exception();

/// Throws a nanonet::util::timeout_exception with a message saying that 
/// op timed out after t seconds.
[[noreturn]] void throw_timeout_exception(double t, const std::string& op);

/// Throws a nanonet::util::shutdown_exception with given message.
[[noreturn]] void throw_shutdown_exception(const std::string& what);

/// Throws an nanonet::util::assertion_failure with given message.
[[noreturn]] void throw_assertion_failure(const std::string& what);

/// Throws a std::runtime_error with given message.
[[noreturn]] void throw_runtime_error(const std::string& what);

/// Throws a nanonet::util::unrecoverable_error with given message.
[[noreturn]] void throw_unrecoverable_error(const std::string& what);

//
// Throws if fun(args...) *does not* throw or text doesn't match its what().
//
// http://en.wikipedia.org/wiki/Variadic_template#C.2B.2B11
//

template< typename F , typename... ARGs > void verify_throws(
    std::string const& text ,
    F const& fun ,
    ARGs... args ) {
  try {
    fun( args... ) ;
  } catch( std::exception const& e ) {
    if( std::string( e.what() ).find( text ) == std::string::npos ) {
      throw std::runtime_error( 
          "exception thrown but what() doesn't match " + text ) ;
    }
    // All OK, exception was thrown and what() matched.
    return;
  }

  // Oops.  We're back after the function call.  Nothing was thrown, so 
  // we throw
  throw std::runtime_error( "expected exception wasn't thrown" ) ;
}

} // namespace util

} // namespace nanonet


/// Always assert that \a expr is true.
///
/// This works like the standard assert() macro, except that:
/// * It can't be switched off by \c -DNDEBUG.
/// * Instead of aborting the process, it throws a
///   nanonet::util::assertion_failure.
#define always_assert( expr )                                       \
  (                                                                 \
    ( expr )      ?                                                 \
    ( void( 0 ) ) :                                                 \
    nanonet::util::assertion( false , #expr , __FILE__ , __LINE__ )     \
  )


/// Debug output.

/// Write \a expr as a string to the output stream \a os, followed
/// by " = " and the \a expr evaluated, followed by '\n'.

#define debug_output( os , expr ) \
  ( ( os ) << #expr << " = " << ( expr ) << '\n' )

/// A testing helper: Execute expr, expects that it throws
/// exception_type whose what() string contains the given text.
#define expect_throws( expr , exception_type , text )                  \
  try { expr ; } catch ( exception_type const& e ) {                   \
    if ( std::string( e.what() ).find( text ) == std::string::npos ) { \
      throw std::runtime_error(                                        \
        std::string( "expected to find: " ) + text                     \
        + ", got: " + e.what() ) ;                                     \
    }                                                                  \
  }

#endif // NANONET_ASSERT_H
