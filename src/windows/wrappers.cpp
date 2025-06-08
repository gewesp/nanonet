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

#include <exception>
#include <stdexcept>
#include <string>
#include <sstream>

#include <cmath>

#include "cpp-lib/util.h"
#include "cpp-lib/sys/file.h"
#include "cpp-lib/sys/util.h"
  
#include "cpp-lib/windows/wrappers.h"


using namespace cpl::detail_ ;
using namespace cpl::util    ;


std::string const cpl::detail_::last_error_message() {

  long const code = GetLastError() ;

  LPVOID lpMsgBuf = 0 ;
  if( 
    !
    FormatMessage( 
	FORMAT_MESSAGE_ALLOCATE_BUFFER 
      | FORMAT_MESSAGE_FROM_SYSTEM 
      | FORMAT_MESSAGE_IGNORE_INSERTS ,
      0 ,
      code ,
      MAKELANGID( LANG_NEUTRAL , SUBLANG_DEFAULT ) ,
      reinterpret_cast< LPTSTR >( &lpMsgBuf ) ,
      0 ,
      0
    )
  )
  { 

	return 
	  "unknown error (Microsoft Windows function FormatMessage() failed)" ;

  }

  std::string ret = reinterpret_cast< char const* >( lpMsgBuf ) ;
  chop( ret ) ;

  LocalFree( lpMsgBuf ) ;
  
  return ret ;

}


void cpl::detail_::last_error_exception( std::string const& msg ) 
{ throw std::runtime_error( msg + ": " + last_error_message() ) ; }


double const cpl::detail_::pc_frequency() {

  LARGE_INTEGER f ;
  QueryPerformanceFrequency( &f ) ;
  return to_double( f ) ;

}


LARGE_INTEGER const cpl::detail_::to_large_integer( double const& x ) {

  always_assert( x >= 0 ) ;

  double const y = std::floor( x + .5 ) ;

  LARGE_INTEGER ret ;

  ret.HighPart = static_cast< LONG  >(            y / 4294967296.   ) ;
  ret.LowPart  = static_cast< DWORD >( std::fmod( y , 4294967296. ) ) ;

  return ret ;

}


double const cpl::detail_::windows_time() {

  static double const f = pc_frequency() ;

  LARGE_INTEGER c ;
  always_assert( QueryPerformanceCounter( &c ) ) ;

  return to_double( c ) / f ;

}


double cpl::util::time() { return windows_time() ; }


double const cpl::detail_::modification_time( HANDLE const fd ) {

  FILETIME tt ;

  if( 0 == GetFileTime( fd , 0 , 0 , &tt ) )
  { last_error_exception( "GetFileTime()" ) ; }

  ULARGE_INTEGER i ;
  i. LowPart = tt. dwLowDateTime ;
  i.HighPart = tt.dwHighDateTime ;

  return to_double( i ) * 1e-7 ;  // 100 nanosecond

}


long cpl::detail_::windows_reader_writer::read
( char      * const buf , long const n ) {

  assert( n >= 1 ) ;
  
  DWORD ret ;

  if( !ReadFile( fd.get() , buf , n , &ret , 0 ) )
  { return -1 ; }

  if( ret <= 0 ) { return -1 ; }

  return ret ;

}


long cpl::detail_::windows_reader_writer::write
( char const* const buf , long const n ) {
  
  assert( n >= 1 ) ;
  
  DWORD ret ;
  if( !WriteFile( fd.get() , buf , n , &ret , 0 ) )
  { return -1 ; }
	
  // TODO: partial send
  assert( n == static_cast< long >( ret ) ) ;

  return ret ;

}


HANDLE cpl::detail_::windows_open( 
  std::string             const& name ,
  std::ios_base::openmode const  om 
) {

  if( om != ( om & ( std::ios_base::out | std::ios_base::in ) ) )
  { throw std::runtime_error( "bad file open mode" ) ; }

  DWORD const acc = 
	  GENERIC_WRITE * ( ( om & std::ios_base::out ) > 0 )
	| GENERIC_READ  * ( ( om & std::ios_base::in  ) > 0 )
	;

  auto_handle fd(
    CreateFile( 
	  name.c_str() , 
	  acc , 
	  FILE_SHARE_READ ,
	  0 , 
	  OPEN_EXISTING , 
	  0 , 
	  0 
	)
  ) ;

  if( !fd.valid() )
  { last_error_exception( "cannot open " + name ) ; }

  return fd.release() ;

}


void cpl::util::sleep( double const& t ) {

  assert( t >= 0 ) ;

  Sleep( static_cast< DWORD >( std::floor( t * 1000 + .5 ) ) ) ;

}


// TODO: This is actually the same implementation as for POSIX.
::timeval const cpl::detail_::to_timeval( double const& t ) { 

  ::timeval ret ; 
  to_fractional( t , 1000000 , ret.tv_sec , ret.tv_usec ) ; 
  return ret ; 

}
