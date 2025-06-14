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


#ifndef CPP_LIB_UTIL_H
#define CPP_LIB_UTIL_H

#include "cpp-lib/assert.h"
#include "cpp-lib/units.h"

#include "boost/algorithm/string.hpp"
#include "boost/algorithm/string/split.hpp"

#include <condition_variable>
#include <fstream>
#include <limits>
#include <memory>
#include <mutex>
#include <ostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

#include <cstdlib>
#include <cmath>
#include <cassert>
#include <cstring>


namespace nanonet {

  
namespace detail_ {

//
// A standard auto_resource_traits class for resource type R.
//

template< typename R >
struct auto_resource_traits ;

/// Resource traits for increment_sentry
template <typename INTEGER>
struct auto_resource_traits_increment_sentry {
  static inline INTEGER* invalid()
  { return nullptr; }
  static inline bool valid     (const INTEGER* const pi) 
  { return pi != nullptr; }
  static inline void initialize(      INTEGER* const pi) 
  { if (valid(pi)) { ++*pi; } }
  static inline void dispose   (      INTEGER* const pi) 
  { if (valid(pi)) { --*pi; } }
};

} // namespace detail_


namespace util {

//
// Return current Universal Time Coordinated [s] since a fixed epoch.
// Clock resolution is ~100Hz.
//
// TODO: The epoch is currently not guaranteed, but will typically be
// 00:00 on January 1, 1970.
//

double utc() ;


//
// Returns the struct tm in UTC based on the given UTC value [s] since
// 00:00 on January 1, 1970.
//

std::tm utc_tm(double utc);


//
// Returns the number of full days since January 1, 1970 for the given utc [s].
//
// Currently only defined for positive UTC values.
//

inline long day_number( double const utc ) {
  assert( utc >= 0 ) ;
  return static_cast< long >( utc / nanonet::units::day() ) ;
}

inline long long llmilliseconds( double const t ) {
  return std::llrint(t * 1000.0);
}

inline const char* time_format_hh_mm() {
  return "%H:%M";
}

inline const char* time_format_hh_mm_ss() {
  return "%H:%M:%S";
}

// Default format for format_datetime()
inline const char* default_datetime_format() {
  return "%FT%TZ";
}

//
// Returns a textual representation of the UTC date/time given in t
// ([s] since January 1, 1970).  The format is as for strftime(3).
// The default format results in an ISO 8601 combined date/time, e.g.:
//
//   2013-04-25T14:50:34Z
//
// Time is rounded to the nearest seconds.
//
// The function throws on errors.
//
// For dates before past_limit, default_result is returned.
//

std::string format_datetime(
    double t, 
    const char* format = default_datetime_format(),
    const double past_limit = -1970 * nanonet::units::year(),
    const char* const default_result = "-");

//
// Same as the above, but format date only, e.g.:
//
//   2013-04-25
//

inline std::string format_date( double const t )
{ return format_datetime(t, "%F"); }


//
// Same as the above, but format time only, e.g.:
//
//   14:50:34Z
//

inline std::string format_time( double const t )
{ return format_datetime(t, "%TZ"); }


//
// Formats dt [s] as [H:]MM[.t] (hours, minutes and
// tenths of minutes (second variant))
//
// If skip_hour is set, only print minutes if dt < 60.
//

std::string format_time_hh_mmt( double const& dt , bool skip_hour = true) ;
std::string format_time_hh_mm ( double const& dt , bool skip_hour = true) ;

//
// Same as the above, but no 'Z' at the end (it's still UTC)
//

inline std::string format_time_no_z( double const t )
{ return format_datetime(t, "%T"); }



//
// Parses a UTC date/time string in ISO8601 format and returns the 
// corresponding number of seconds since 00:00 on January 1, 1970.
//
// Example time string:
//
//   2013-04-25T14:50:34Z
//
// TODO: Use standardized functions!
//
// The function throws on parse and other errors.
//

double parse_datetime( 
    std::string const& , const char* const format = "%FT%TZ" ) ;

// Check if argument is an integer and is in [ min , max ], throw an
// exception if not.
long check_long
( double const& x , double const& min , double const& max ) ;


/// This tag can be used as a constructor argument to
/// signal that the constructor leaves fields uninitialized.
struct uninitialized {} ;


//
// Marks a value as being unused in order to avoid compiler warnings.
//

template< typename T >
void mark_unused( T const& t ) {
  static_cast< void >( t ) ;
}

//
// A safe getline() function with maximum number of characters.  Should
// be used if the input source is not trusted.
//
// CAUTION: Checks for '\n' only (not for '\r'); just as std::getline().
//
// Precondition: maxsize > 0
// Postcondition: s.size() <= maxsize
//
// A line longer than maxsize runs over into the next line.
//
// A line that is exactly maxsize long followed by '\n' causes
// an additional empty line to be read.
//
// size_hint can be used to tell the function to expect strings of
// approximately that size.
//

std::istream& getline(std::istream&, std::string& s, long maxsize,
    long size_hint = 0);

//
// Splits s on separator and inserts it into the given sequence.
//
// Usage e.g.:
//   std::vector< std::string > strs;
//   nanonet::util::split(strs, "x,y,2");
// Result: a vector containing "x", "y" and "2".
//

template< typename Sequence > void split( 
    Sequence& seq , std::string const& s , char const* const separators = "," ) {
  boost::algorithm::split(seq, s, boost::algorithm::is_any_of(separators));
}

/// @return Splits two elements on on separator and returns result.
/// Convenience shortcut for split(sequence, s, separators).
std::pair<std::string, std::string> split_pair(
    std::string const& s,
    char const* separators = ",");

/// Splits strings of the form "<s1>: <s2>", <s1> is followed
/// by a colon immediately, then there may be whitespace before
/// value.  For example, split_colon_blank("Content-Type: text/html")
/// would return a pair of "Content-Type" and "text/html".
std::pair<std::string, std::string> split_colon_blank(std::string const& s);

//
// A string splitter returning the next string on each call.
//
// Usage:
//   splitter split(input_string);
//   std::string part;
//   while (s.get_next(part)) { ... }
//

struct splitter {
  splitter(const std::string& str, const char separator = ',')
  : position_ (std::begin(str)),
    end_      (std::end  (str)),
    separator_(separator)
  {}

  // Invariant: position_ == end_ or there is a separator
  // left of position_
  bool get_next(std::string& result) {
    if (exhausted_) {
      return false;
    }
    const auto it = std::find(position_, end_, separator_);
    result.assign(position_, it);
    if (it == end_) { 
      // Log that we're done
      exhausted_ = true;
    } else {
      position_ = it + 1;
    }
    return true;
  }

private:
  std::string::const_iterator position_;
  std::string::const_iterator end_     ;
  char separator_;
  // Need a separate flag for exhausted since position_ can be on
  // end_ for an empty input string
  bool exhausted_ = false;
};

//
// A second implementation for cross-checking, based on splitter
//

std::vector<std::string> split(
    std::string const& s , char separator = ',');


//
// Copy is to os character-wise
//

inline void stream_copy( std::istream& is , std::ostream& os ) {

  std::copy(
    std::istreambuf_iterator< char >( is ) ,
    std::istreambuf_iterator< char >(    ) ,
    std::ostreambuf_iterator< char >( os )
  ) ;

}


///
/// @return True iff name equals "-", "stdin" or "STDIN".
///

bool is_stdin (const std::string& name);

///
/// @return True iff name equals "-", "stdout" or "STDOUT".
///

bool is_stdout(const std::string& name);


//
// File helpers
//

namespace file {

//
// Using
// http://en.wikipedia.org/wiki/Curiously_recurring_template_pattern
// with a traits class to enable owning_istream/owning_ostream constructors
// with a buffer_maker<> argument that can be e.g. a network connection.
//
// The traits class is necessary because the derived class is considered
// incomplete upon deriving from the template:
// http://stackoverflow.com/questions/6006614/c-static-polymorphism-crtp-and-using-typedefs-from-derived-classes
//
// Usage example:
// struct connection : buffer_maker<connection> { ... } ;
// namespace file { 
//   template<> struct buffer_maker_traits<connection> { ... } ;
// }
//

// The traits class must define istreambuf_type and ostreambuf_type.
template< typename IO > struct buffer_maker_traits {} ;

// Type IO must define:
//   traits::istreambuf_type make_istreambuf() 
//   traits::ostreambuf_type make_ostreambuf() 
template< typename IO , typename traits = buffer_maker_traits< IO > >
struct buffer_maker {
  IO& downcast() { return *static_cast< IO* >( this ) ; }
  typename traits::istreambuf_type make_istreambuf() 
  { return downcast().make_istreambuf() ; }
  typename traits::ostreambuf_type make_ostreambuf() 
  { return downcast().make_ostreambuf() ; }
} ;

//
// owning_istream and owning_ostream are stream classes which
//
// 1. Always have a buffer.
// 2. Delete their current buffer on destruction.
//
// Notice that rdbuf() is a non-virtual function.  Do *not* call it
// on owning streams.
//
// Rationale: Is there any sense in having a stream without a buffer?
//
// These classes are a bit experimental as of 2014, in particular
// in view of slow adoption of move semantics for streams.
//

template< typename T >
struct owning_istream : std::istream {
  // Construct from buffer maker
  template< typename IO >
  owning_istream( buffer_maker< IO >& bm )
  : owning_istream( bm.make_istreambuf() ) {}

  // Construct from a buffer
  owning_istream( T&& buf )
  : std::istream( NULL ) , mybuf( std::move( buf ) )
  { rdbuf( &mybuf ) ; assert( rdbuf() ) ; }

  // Noncopyable
  owning_istream           ( owning_istream const& ) = delete;
  owning_istream& operator=( owning_istream const& ) = delete;

  // Moveable
  owning_istream( owning_istream&& other )
  : std::istream( std::move( other ) ) , mybuf( std::move( other.mybuf ) ) { 
    rdbuf( &mybuf ) ; 
  }

  owning_istream& operator=( owning_istream&& rhs ) {
    std::istream::operator=( std::move( rhs ) ) ;
    mybuf = std::move( rhs.mybuf ) ;
    rdbuf( &mybuf ) ;
    return *this;
  }

  T&       buffer()       { return mybuf; }
  T const& buffer() const { return mybuf; }

private:
  // This is mine!
  T mybuf ;

} ;


// Apparently, std::ostream *does* have a default constructor 
// for clang++-3.5 ?!  Using it resulted in disaster, however.
// So, initialize to NULL and immediately call rdbuf().
template< typename T >
struct owning_ostream : std::ostream {
  // Construct from buffer maker
  template< typename IO >
  owning_ostream( buffer_maker< IO >& bm )
  : owning_ostream( bm.make_ostreambuf() ) {}

  // Construct from buffer
  owning_ostream( T&& buf )
  : std::ostream( NULL ) , mybuf( std::move( buf ) )
  { rdbuf( &mybuf ) ; assert( rdbuf() ) ; }

  // Noncopyable
  owning_ostream           ( owning_ostream const& ) = delete ;
  owning_ostream& operator=( owning_ostream const& ) = delete ;

  // Moveable
  owning_ostream( owning_ostream&& other )
  : std::ostream( NULL ) , mybuf( std::move( other.mybuf ) ) 
  { rdbuf( &mybuf ) ; }

  owning_ostream& operator=( owning_ostream&& rhs ) {
    // http://www.cplusplus.com/reference/ostream/ostream/operator=/
    // 'same behaviour as calling member ostream::swap'
    // Does *not* exchange rdbuf() pointers.
    flush(); // is this needed?  
    std::ostream::operator=( std::move( rhs ) ) ;
    mybuf = std::move( rhs.mybuf ) ;
    rdbuf( &mybuf ) ;
    return *this;
  }

  T&       buffer()       { return mybuf; }
  T const& buffer() const { return mybuf; }

private:
  T mybuf ;

} ;

} // namespace file


//
// Toggle a boolean.
//

inline void toggle( bool& b ) { b = !b ; }


//
// Chop whitespace at the end of a string.
//

void chop( std::string& ) ;

//
// tolower/toupper for strings
//

inline void tolower(std::string& s)
{ for (char& c : s) { c = std::tolower(c); } }
inline void toupper(std::string& s)
{ for (char& c : s) { c = std::toupper(c); } }


//
// UTF-8 capable tolower/toupper
//
// Uses the en_US.UTF-8 locale.
//

std::string utf8_tolower(std::string const& s);
std::string utf8_toupper(std::string const& s);


//
// Removes any non-alphanumeric or non-extra characters from s
// and returns the result.  If convert == 1, also converts all to
// upper case.  If convert == -1, also converts all to lower case.
// If convert == 0, doesn't do any case conversion.
//

std::string utf8_canonical(
    std::string const& s,
    std::string const& extra = std::string(),
    int convert = 0);

/// Verifies that a string contains only alphanumeric and
/// possibly extra characters.  If throw_on_invalid is true,
/// throws nanonet::util::value_error on violation.  Otherwise,
/// returns false.
bool verify_alnum(
    std::string const& s, std::string const& extra = std::string(),
    bool throw_on_invalid = true);

// Remove any non-alphanumeric or non-extra characters from s
// and convert the remaining ones to upper case.  Returns the
// result.
std::string canonical(std::string const& s, std::string const& extra = "");


//
// Convert a (time) value t >= 0 into its integral part s and fractional
// part f, 0 <= f < m such that
//
//   t approximately equals s + f/m.
//
// S and F should be integral types.
//

template< typename S , typename F >
void to_fractional( double const& t , long const m , S& s , F& f ) {

  assert( m > 0 ) ;

  always_assert( t >= 0 ) ;

  // Round.
  double const tt = std::floor( t * m + .5 ) / m ;

  always_assert( tt >= 0 ) ;

  double const i = std::floor( tt ) ;

  s = static_cast< S >( i ) ;

  double const ff = m * ( tt - i ) ;
  always_assert( 0 <= ff       ) ;
  always_assert(      ff < m ) ;

  f = static_cast< F >( ff + .5 ) ;
  always_assert( 0 <= f     ) ;
  always_assert(      f < m ) ;

}


//
// A dirty hack to clear out an arbitrary structure.
//

template< typename T >
void clear( T& t ) {

  char* adr = reinterpret_cast< char* >( &t ) ;

  std::fill( adr , adr + sizeof( T ) , 0 ) ;

}

//
// A hopefully safe memcmp wrapper, compares byte-wise
//
// Returns: True iff both t1 and t2 are byte-wise equal.
//

template< typename T >
bool mem_equal( T const& t1 , T const& t2 ) {

  void const* const a1 = reinterpret_cast< void const* >( &t1 ) ;
  void const* const a2 = reinterpret_cast< void const* >( &t2 ) ;

  return 0 == memcmp( a1 , a2 , sizeof( T ) ) ;
}

//
// Unidirectional stream buffers based on the reader_writer concept.
//
// A reader_writer class holds a resource (e.g., file descriptor,
// socket, ...) and must expose two methods:
//
//   long read ( char      * const buf , long const n ) ;
//   long write( char const* const buf , long const n ) ;
//
// Both return either the number of characters read or written or -1
// on error.
//
// n is the number of bytes to be written or read and must be >= 1.
//
// read() tries to read at most n characters.  A return value of zero
// indicates end of file.
//
// write() tries to write n characters.  The return value is either n or
// -1.
//
// The buffer co-owns the reader_writer with the caller.
// This allows for multiple buffers to be used on a single resource, 
// e.g. an input and an output buffer on a network socket.
//
// This implementation is based on code by Nico Josuttis which was 
// accompanied by the following notice:
/*
 *
 * (C) Copyright Nicolai M. Josuttis 2001.
 * Permission to copy, use, modify, sell and distribute this software
 * is granted provided this copyright notice appears in all copies.
 * This software is provided "as is" without express or implied
 * warranty, and with no claim as to its suitability for any purpose.
 *
 */

// 
// For implementation of streambufs, see also the newsgroup postings of
// Dietmar Kuehl, in particular:
//   http://groups.google.com/groups?hl=en&lr=&ie=UTF-8&oe=UTF-8&selm=5b15f8fd.0309250235.3200cd43%40posting.google.com
//
// and
//
//   Josuttis, The C++ Standard Library, Chapter 13 (p. 678)
//
// The relevant standard chapter: 25.1.
//
// See also the crypt_istreambuf implementation.
//
// TODO: Implement xsputn().
// TODO: Maybe get rid of virtual inheritance.
//       In this case, the 'most derived' class initializes the virtual base,
//       causing significant confusion.
//       See http://stackoverflow.com/questions/27817478/virtual-inheritance-and-move-constructors
//

// From Nico's original comment:
/*
 * - open:
 *      - integrating BUFSIZ on some systems?
 *      - optimized reading of multiple characters
 *      - stream for reading AND writing
 *      - i18n
 */

template
< typename RW >
struct ostreambuf : std::streambuf {

  // TODO: Move assignment---also for istreambuf
  ostreambuf( ostreambuf const& ) = delete ;

  ostreambuf( ostreambuf&& other )
    : std::streambuf( static_cast<std::streambuf const&>( other ) ) ,
      rw    ( std::move( other.rw     ) ) ,
      buffer( std::move( other.buffer ) ) {

    // Important!! Invalidate other so that destructor becomes a NO-OP.
    // The destructor *is* being called after an object has been
    // moved from.
    other.setp(NULL, NULL);
  }

  ostreambuf& operator=(ostreambuf      &&) = delete;
  ostreambuf& operator=(ostreambuf const& ) = delete;

  //
  // Construct an ostreambuf on resource r with buffer size.
  //

  ostreambuf( std::shared_ptr<RW> const rw , int const size = 1024 )
  : rw    ( rw   ) ,
    buffer( size )
  { 
    
    always_assert( size >= 1 ) ; 
    setp( &buffer[ 0 ] , &buffer[ 0 ] + size - 1 ) ; 

  }

  ostreambuf( int const size = 1024 )
  : rw    ( NULL  ) ,
    buffer(  size ) {

    always_assert( size >= 1 ) ; 
    setp( &buffer[ 0 ] , &buffer[ 0 ] + size - 1 ) ; 

  }

  // Move constructor leaves rw (shared_ptr) empty.
  virtual ~ostreambuf() { 
    if (rw && pbase()) {
      sync() ; 
      rw->shutdown_write() ;
    }
  }

  void set_reader_writer( std::shared_ptr<RW> const newrw ) { 
    rw = newrw ;
  }

  RW& reader_writer() {
    assert( rw ) ;
    return *rw ;
  }

  RW const& reader_writer() const {
    assert( rw ) ;
    return *rw ;
  }

protected:

  //
  // See 27.5.2.4.5p3
  // Failure: EOF, success: != EOF.
  //

  virtual int_type overflow( int_type c ) {

    if( traits_type::eof() != c ) {

      *pptr() = traits_type::to_char_type( c ) ;
      pbump( 1 ) ;

    }

    return flush() ? c : traits_type::eof() ;

  }

  //
  // See 27.5.2.4.2p7. ``Returns -1 on failure''.
  //

  virtual int sync() { return flush() ? 0 : -1 ; }


private:

  //
  // Like sync(), but return a bool (true on success).
  //

  bool flush() ;

  std::shared_ptr<RW> rw ; 
  
  std::vector< char > buffer ;

} ;


template< typename RW >
struct istreambuf : virtual std::streambuf {

  istreambuf( istreambuf const& ) = delete ;

  istreambuf( istreambuf&& other )
    : std::streambuf( static_cast<std::streambuf const&>( other ) ) ,
      rw     ( std::move( other.rw      ) ) ,
      size_pb( std::move( other.size_pb ) ) ,
      buffer ( std::move( other.buffer  ) ) {

    // Important!! Invalidate other so that destructor becomes a NO-OP.
    // The destructor *is* being called after an object has been
    // moved from.
    other.setp(NULL, NULL);
  }

  istreambuf& operator=(istreambuf      &&) = delete;
  istreambuf& operator=(istreambuf const& ) = delete;

  
  //
  // Construct an istreambuf on resource r, buffer
  // size size, and putback area size size_pb.
  //

  istreambuf( 
    std::shared_ptr<RW> rw   , 
    int const size    = 1024 ,
    int const size_pb = 4 
  ) 
  : rw( rw ) ,
    size_pb( size_pb ) ,
    buffer( size + size_pb , 0 ) {

    always_assert( size    >= 1 ) ;
    always_assert( size_pb >= 1 ) ;
   
    char* const p = &buffer[ 0 ] + size_pb ; 
    setg( p , p , p ) ;

  }

  RW& reader_writer() {
    assert( rw ) ;
    return *rw ;
  }

  RW const& reader_writer() const {
    assert( rw ) ;
    return *rw ;
  }
  
  // Move constructor leaves rw (shared_ptr) empty.
  virtual ~istreambuf() 
  { if (rw) { rw->shutdown_read() ; } }

protected:

  virtual int_type underflow() ;

private:

  std::shared_ptr<RW> rw ;

  // Size of putback area.
  int const size_pb ; 

  std::vector< char > buffer ;

} ;


//
// A resource manager template with clean RAII and move semantics.
// It is designed to handle the typical Operating System specific
// handles like UNIX file descriptors, BSD sockets, Windows handles,
// etc.  Also, it can be used for sentry types, see increment_sentry.
// The handle type is designated by 'R' below.
//
// It requires a traits class T with static methods
//
//   R    T::invalid()             -- Return an ``invalid'' resource 
//                                    handle (e.g., -1 for file handles).
//   bool T::valid( R const& h )   -- Return true iff handle h
//                                    is valid. 
//   void T::initialize( R const& h ) -- Does any initializing work on h,
//                                       may be a NO-OP
//   void T::dispose( R const& h ) -- Return h to the OS.
//

template
< typename R , typename T = nanonet::detail_::auto_resource_traits< R > >
struct auto_resource {

  auto_resource(                   ) : h( T::invalid()  ) {}
  auto_resource( R const h_in      ) : h( h_in          ) { T::initialize( h ) ; }

  // Move
  auto_resource( auto_resource&& other ) : h( other.release() ) {}
  auto_resource& operator=( auto_resource&& rhs ) 
  { h = rhs.release() ; return *this ; }

  // Noncopyable
  auto_resource& operator=( auto_resource const& ) = delete ;
  auto_resource           ( auto_resource const& ) = delete ;

  R get() const { return h ; }

  bool valid() const { return T::valid( h ) ; }

  void reset( R const hh = T::invalid() ) { dispose() ; h = hh ; }

  ~auto_resource() { dispose() ; }

private:

  void dispose() const { if( T::valid( h ) ) { T::dispose( h ) ; } }

  R release() 
  { R const ret = h ; h = T::invalid() ; return ret ; }

  R h ;

} ;


/// A moveable, non-copyable 'increment' sentry whose
/// constructor increases value by one and its destructor
/// decreases it again.
template <typename INTEGER>
struct increment_sentry {
  /// Increases value by one
  increment_sentry(INTEGER& value)
  : ar_(&value) {}

private:
  auto_resource<
    INTEGER*, 
    detail_::auto_resource_traits_increment_sentry<INTEGER>> ar_;
};

} // namespace util

} // namespace nanonet


//
// Template definitions.
//

template< typename RW >
bool nanonet::util::ostreambuf< RW >::flush() {

  if (!rw) { return true ; }

  long const n = pptr() - pbase() ;

  assert( 0 <= n                                         ) ;
  assert(      n <= static_cast< long >( buffer.size() ) ) ;

  if( 0 == n ) { return true ; }

  long const written = rw->write( pbase() , n ) ;

  if( written < 0 ) { return false ; }

  assert( n == written ) ;

  pbump( -n ) ;
  assert( pptr() == &buffer[ 0 ] ) ; 
  return true ;

}


template< typename RW >
typename nanonet::util::istreambuf< RW >::int_type
nanonet::util::istreambuf< RW >::underflow() {

  if( gptr() < egptr() ) 
  { return traits_type::to_int_type( *gptr() ) ; }

  int const n_pb = 
    std::min( 
      static_cast< long >( gptr() - eback() ) , 
      static_cast< long >( size_pb          ) 
    ) ;

  std::memmove( &buffer[ 0 ] + size_pb - n_pb , gptr() - n_pb , n_pb ) ;

  long const read = 
    rw->read( &buffer[ 0 ] + size_pb , buffer.size() - size_pb ) ;
  
  if( read <= 0 ) { return traits_type::eof() ; }

  setg(
    &buffer[ 0 ] + size_pb - n_pb , // beginning of putback area
    &buffer[ 0 ] + size_pb        , // read position
    &buffer[ 0 ] + size_pb + read   // end of buffer
  ) ;

  return traits_type::to_int_type( *gptr() ) ;

}

#endif // CPP_LIB_UTIL_H
