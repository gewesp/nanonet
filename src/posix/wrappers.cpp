//
// This is only compiled on POSIX platforms
//

#include "nanonet/util.h"
#include "nanonet/posix/wrappers.h"
#include "nanonet/sys/util.h"
#include "nanonet/detail/platform_definition.h"

#include <exception>
#include <stdexcept>
#include <string>
#include <sstream>

#include <cmath>
#include <cstring>
#include <cerrno>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include <string.h>


namespace {

#if (BOOST_OS_LINUX)
// TODO: We want a flag to indicate we require a normal file.
constexpr int OPEN_FLAGS_NORMAL_FILE = 0;
#elif (BOOST_OS_MACOS)
constexpr int OPEN_FLAGS_NORMAL_FILE = 0;
#endif

} // anonymous namespace

using namespace nanonet::detail_ ;
using namespace nanonet::util    ;


// Errno is thread safe.
// http://stackoverflow.com/questions/1694164/is-errno-thread-safe
void nanonet::detail_::strerror_exception
( std::string const& s , int const errnum ) {

  throw std::runtime_error
  ( s + ": " + get_strerror_message( errnum ? errnum : errno ) ) ;

}

std::string nanonet::detail_::get_strerror_message( int const errnum ) {
  int constexpr OS_ERROR_BUFSIZE = 1024;
  char v[OS_ERROR_BUFSIZE] = {0};

  // Argh.
  // As of 2020, we need to live with the fact that there are two
  // versions of strerror_r().  One (XSI compliant) returns an int, 
  // the other one (GNU specific) returns a char*.  Apparently, the
  // GNU version is in much wider use.
  // TODO: Have that in a separate file with _GNU_SOURCE undefined,
  // or get rid of _GNU_SOURCE finally?!
#if (not defined(__APPLE__)) and ((_POSIX_C_SOURCE < 200112L) || _GNU_SOURCE)
  const char* const res = ::strerror_r(errnum, &v[0], OS_ERROR_BUFSIZE);
  if (res) {
    return res;
  }
#else
  // XSI-compliant version, actually *preferred* according to the Linux
  // man page.  However, basically unusable because too much infrastructure
  // depends on _GNU_SOURCE being defined.
  int const res = ::strerror_r(errnum, &v[0], OS_ERROR_BUFSIZE);

  // "The XSI-compliant strerror_r() function returns 0 on success."
  if (0 == res) { 
    return v; 
  }
#endif

  else {
    return "WTF: strerror_r() call failed for error code: "
      + std::to_string(errnum);
  }
}


double nanonet::detail_::modification_time( int const fd ) {

  struct stat buf ;

  if( ::fstat( fd , &buf ) < 0 ) { strerror_exception( "fstat" ) ; }

  return buf.st_mtime ;

}


int nanonet::detail_::posix_open( 
  std::string             const& name ,
  std::ios_base::openmode const  om 
) {

  if( 0 == om || om != ( om & ( std::ios_base::out | std::ios_base::in ) ) )
  { throw std::runtime_error( "bad file open mode" ) ; }

  int flags = 0 ;
  if     ( std::ios_base::out == om ) { flags = O_WRONLY ; }
  else if( std::ios_base::in  == om ) { flags = O_RDONLY ; }
  else                                { flags = O_RDWR   ; }

  flags |= ::OPEN_FLAGS_NORMAL_FILE;

  int fd = ::open( name.c_str() , flags ) ;

  if( fd < 0 ) { strerror_exception( "open " + name ) ; }

  return fd ;

}


void nanonet::util::sleep( double const& t ) {

  assert( t >= 0 ) ;

  ::timespec const tt = to_timespec( t ) ;

  if( -1 == ::nanosleep( &tt , 0 ) )
  { strerror_exception( "sleep" ) ; }

}


double nanonet::util::time() {

  ::timeval t ;

  int const err = ::gettimeofday( &t , 0 ) ;
  if( err ) { strerror_exception( "gettimeofday()" , err ) ; }
  return to_double( t ) ;

}


::sigset_t nanonet::detail_::block_signal( int const s ) {

  ::sigset_t ret ;

  // sigemptyset() and sigaddset() may be macros!
  STRERROR_CHECK( sigemptyset( &ret     ) ) ;
  STRERROR_CHECK( sigaddset  ( &ret , s ) ) ;

  int const err = ::pthread_sigmask( SIG_BLOCK , &ret , 0 ) ;
  if( err ) { strerror_exception( "pthread_sigmask()" , err ) ; }

  return ret ;
 
}


long nanonet::detail_::posix_reader_writer::read(char* const buf, long const n) {
  
  long ret ;
  do { ret = ::read( fd.get() , buf , n ) ; } 
  while( EINTR_repeat( ret ) ) ;

  assert( -1 <= ret      ) ;
  assert(       ret <= n ) ;

  return ret ;

}


long nanonet::detail_::posix_reader_writer::write(
    char const* const buf, long const n) {
  
  long ret ;

  // TODO: partial send
  do { ret = ::write( fd.get() , buf , n ) ; } 
  while( EINTR_repeat( ret ) ) ;

  if( ret < 0 ) 
  { return -1 ; }

  assert( n == ret ) ;
  return ret ;

}

::timespec const nanonet::detail_::to_timespec( double const& t ) {

  ::timespec ret ;
  to_fractional( t , 1000000000 , ret.tv_sec , ret.tv_nsec ) ;
  return ret ;

}

::timeval const nanonet::detail_::to_timeval( double const& t ) {

  ::timeval ret ;
  to_fractional( t , 1000000 , ret.tv_sec , ret.tv_usec ) ;
  return ret ;

}
