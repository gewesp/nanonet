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

#include "cpp-lib/util.h"

#include "boost/io/ios_state.hpp"

#include <chrono>
#include <codecvt>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <memory>
#include <exception>
#include <stdexcept>
#include <limits>
#include <locale>

#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "cpp-lib/error.h"
#include "cpp-lib/exception.h"

#include "cpp-lib/detail/platform_wrappers.h"
#include "cpp-lib/sys/syslogger.h"


using namespace cpl::util ;


namespace {

std::ostream* die_output = 0 ;

std::string const die_output_name() { return "CPP_LIB_DIE_OUTPUT" ; }

// Locale to use for UTF-8 stuff.
// Argh. C.UTF-8 doesn't work on modern MacOS
// std::locale const utf8_locale("C.UTF-8");
std::locale const utf8_locale("en_US.UTF-8");

// UTF-8 <-> std::wstring conversion
// We do not want wstring in public interfaces, but 
// internally it is needed for operations like toupper, tolower.
std::wstring to_wstring(std::string const& s) {
  // Converters do have state.
  std::wstring_convert<std::codecvt_utf8<wchar_t> > conv;
  return conv.from_bytes(s);
}

std::string to_string(std::wstring const& s) {
  std::wstring_convert<std::codecvt_utf8<wchar_t> > conv;
  return conv.to_bytes(s);
}

std::string const allowed_characters_1_ = ",.-/() ";
std::string const allowed_characters_2_ = ",.:/-"  ;
std::string const allowed_characters_3_ = "-"      ;
std::string const allowed_characters_4_ = "-_"     ;

} // anonymous namespace


void cpl::util::set_die_output( std::ostream* os )
{ die_output = os ; }

// TODO: Templatize, etc?
// http://en.cppreference.com/w/cpp/string/basic_string/getline
std::istream& cpl::util::getline(
    std::istream& is , std::string& s , 
    long const maxsize , long const size_hint ) {

  always_assert( maxsize > 0 ) ;

  // Do *not* skip whitespace!!
  std::istream::sentry sen( is , true ) ;
  if ( !sen ) {
    return is ;
  }

  s.clear() ;
  s.reserve( size_hint ) ;

  std::istreambuf_iterator< char > it( is ) ;
  std::istreambuf_iterator< char > const eos ;

  long i = 0 ;

  while( i < maxsize ) {
    if ( eos == it ) {
      is.setstate( std::ios_base::failbit | std::ios_base::eofbit ) ;
      return is ;
    }
    char const c = *it ;
    ++it ;
    if( '\n' == c ) {
      return is ;
    } else {
      s.push_back( c ) ;
      ++i ;
    }
  }

  return is ;
}

std::vector<std::string> cpl::util::split(
    std::string const& str,
    char const sep) {
  std::string back;
  cpl::util::splitter s(str, sep);

  std::vector<std::string> ret;
  while (s.get_next(back)) {
    ret.push_back(back);
  }

  return ret;
}

std::pair<std::string, std::string> cpl::util::split_pair(
    std::string const& s,
    const char* const separators) {
  std::vector<std::string> v;
  cpl::util::split(v, s, separators);
  cpl::util::verify(2 == v.size(),
      "split_pair(): Expected 2 fields, got " + std::to_string(v.size()));
  return std::make_pair(v[0], v[1]);
}

std::pair<std::string, std::string> cpl::util::split_colon_blank(
    std::string const& s) {
  std::pair<std::string, std::string> ret;

  const auto colon = s.find(':');
  assert(s.end() != s.begin() + colon);

  if (std::string::npos == colon) {
    util::throw_error("split_colon_blank(): No colon found: " + s);
  }

  ret.first = s.substr(0, colon);

  const auto second_start =
      std::find_if(s.begin() + colon + 1, s.end(),
          [] (const char c) { return not (' ' == c or '\t' == c); });

  ret.second = std::string(second_start, s.end());

  return ret;
}

death::death()
: os( nullptr )
{}

void cpl::util::death::die(
  std::string msg ,
  std::string name ,
  int const exit_code
) {

  using namespace cpl::util::log;

  if( nullptr != os ) {
    *os << prio::EMERG << msg << std::endl ;
  } else {
    try {
      syslogger sl ;
      sl << prio::EMERG << msg << std::endl ;
    } catch ( std::exception const& e ) {
      msg += "(Writing to syslog failed: " ;
      msg += e.what()                      ;
      msg += ")"                           ;
    }
  }

  std::cerr << prio::EMERG << msg << std::endl;
  std::clog << prio::EMERG << msg << std::endl;

  if( name == "" )
  { name = die_output_name() ; }

  {
    // If the following fails, we're really f**cked up :-)
    std::ofstream last_chance( name.c_str() ) ;
    last_chance << prio::EMERG << msg << std::endl ;
  }

  this->exit( exit_code ) ;
}

void cpl::util::die(
  std::string const& msg ,
  std::string name ,
  int const exit_code
)
{ death( die_output ).die( msg , name , exit_code ) ; }

bool cpl::util::is_stdin(const std::string& f) {
  return "-" == f or "stdin" == f or "STDIN" == f;
}

bool cpl::util::is_stdout(const std::string& f) {
  return "-" == f or "stdout" == f or "STDOUT" == f;
}

void cpl::util::assertion(
  const bool expr ,
  std::string const& expr_string ,
  std::string const& file ,
  const long line
) {
  // expr is true -> All OK
  if (expr) {
    return;
  }

  cpl::util::throw_assertion_failure(
        "Assertion failed: " 
      + expr_string 
      + " (" + file + ":" + std::to_string(line) + ")");
}

void cpl::util::verify(
  const bool expression ,
  std::string const& message
) { 
  if( !expression ) { throw std::runtime_error( message ) ; }
}


long cpl::util::check_long
( double const& x , double const& min , double const& max ) {

  if( x < min || x > max ) {

    std::ostringstream os ;
    os << "should be between " << min << " and " << max ;
    throw std::runtime_error( os.str() ) ;

  }

  if( static_cast< long >( x ) != x )
  { throw std::runtime_error( "should be an integer" ) ; }

  return static_cast< long >( x ) ;

}


void cpl::util::scan_past( std::istream& is , char const* const s ) {

  char c ;

restart:

  char const* p = s ;

  while( *p && is.get( c ) ) {

    if( *p != c ) { goto restart ; }
    ++p ;

  }

}

double cpl::util::utc() {
  return 
    std::chrono::duration<double>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

std::tm cpl::util::utc_tm(double const t) {
  std::time_t const t1 = t + 0.5;
  std::tm ret;
  // Better safe than sorry: Set members to 0
  cpl::util::clear(ret);
  if (NULL == ::gmtime_r(&t1, &ret)) {
    cpl::detail_::strerror_exception("gmtime()", errno);
  }
  return ret;
}

std::string cpl::util::format_datetime(
    double const t, char const* const format,
    double const past_limit, char const* const default_result) {
  if (t < past_limit) {
    return default_result;
  }

  std::tm t2 = cpl::util::utc_tm(t);

  char ret[100];
  if (0 == strftime(ret, 99, format, &t2)) {
    throw std::runtime_error("format_datetime(): formatting failure");
  }
  return ret;
}

std::string cpl::util::format_time_hh_mm(double const& dt,
                                         bool const skip_hour) {
  assert( dt >= 0 ) ;

  double h = std::floor(dt / cpl::units::hour());
  double m = std::round((dt - h * cpl::units::hour()) / cpl::units::minute());
  if (m >= 59.99) {
    m = 0;
    ++h;
  }

  char ret[30];
  if (h < 0.1 && skip_hour) {
    sprintf(ret, "%02.0lf", m);
  } else {
    sprintf(ret, "%.0lf:%02.0lf", h, m);
  }
  return ret;
}


std::string cpl::util::format_time_hh_mmt(double const& dt,
                                          bool const skip_hour) {
    
  assert( dt >= 0 ) ;

  double h = std::floor(dt / cpl::units::hour());
  // Round to nearest 10th of minute
  double m = 0.1 * std::round(
      10 * (dt - h * cpl::units::hour()) / cpl::units::minute());
  if (m >= 59.99) {
    m = 0;
    ++h;
  }

  char ret[30];
  if (h < 0.1 && skip_hour) {
    sprintf(ret, "%04.1lf", m);
  } else {
    sprintf(ret, "%.0lf:%04.1lf", h, m);
  }
  return ret;
}

double cpl::util::parse_datetime(
    std::string const& s, char const* format) {
  std::tm t2;

  // Set members to 0.  Absolutely necessary here since strptime()
  // may leave fields untouched, depending on the format.
  cpl::util::clear(t2);

  const char* const buf = s.c_str();
  const char* const res = strptime(buf, format, &t2);

  if (NULL == res) {
    throw std::runtime_error("parse_datetime(): parse error: " + s);
  }

  if (buf + s.size() != res) {
    throw std::runtime_error("parse_datetime(): parsing incomplete: " + s);
  }

  // from man mktime(3):
  // The timegm() function is not specified by any standard; its function can-
  // not be completely emulated using the standard functions described above.
  return timegm(&t2);
}


bool cpl::util::simple_scheduler::action( double const& t ) {

  if( t_last <= t && t < t_last + dt ) { return false ; }
  t_last = t ; return true ;

}


// The cast is necessary because std::isspace(int) is undefined for
// value other than EOF or unsigned char

void cpl::util::chop( std::string& s ) {

  std::size_t i ;
  for(
    i = s.size() ;
    i > 0 && std::isspace( static_cast< unsigned char >( s[ i - 1 ] ) ) ;
    --i
  )
  {}

  s.resize( i ) ;

}

bool cpl::util::verify_alnum(std::string const& s, std::string const& extra, const bool throw_on_invalid) {
  for (char c : s) {
    if (not std::isalnum(c) && std::string::npos == extra.find(c)) {
      if (throw_on_invalid) {
        throw cpl::util::value_error(
              "invalid character in " + s 
            + ": must be alphanumeric or in " + extra);
      } else {
        return false;
      }
    }
  }
  return true;
}

std::string cpl::util::canonical(
    std::string const& s, std::string const& extra) {
  std::string ret;
  for (char c : s) {
    c = std::toupper(c);
    if (std::isalnum(c) || std::string::npos != extra.find(c)) {
      ret.push_back(c);
    }
  }
  return ret;
}

// TODO: Is this thread-safe?  I guess OK, as long as no
// COW is used...
std::string const& cpl::util::allowed_characters_1() {
  return allowed_characters_1_;
}

std::string const& cpl::util::allowed_characters_2() {
  return allowed_characters_2_;
}

std::string const& cpl::util::allowed_characters_3() {
  return allowed_characters_3_;
}

std::string const& cpl::util::allowed_characters_4() {
  return allowed_characters_4_;
}

////////////////////////////////////////////////////////////////////////
// JSON stuff
////////////////////////////////////////////////////////////////////////
namespace {

inline void json_escape_char(std::ostream& os, const char c) {
  switch (c) {
    case '"' : os << "\\\""; break;
    case '\\': os << "\\\\"; break;
    case '\b': os << "\\b" ; break;
    case '\f': os << "\\f" ; break;
    case '\n': os << "\\n" ; break;
    case '\r': os << "\\r" ; break;
    case '\t': os << "\\t" ; break;

    default:
      if ('\x00' <= c && c <= '\x1f') {
        os << "\\u"
           << std::hex << std::setw(4) << std::setfill('0')
           << static_cast<int>(c);
      } else {
        os << c;
      }
      break;
  }
}

} // anonymous namespace

std::ostream& cpl::util::json_escape(std::ostream& os, const char* str) {
  const char* c = str;

  // FIXME: Factor out that state saving
  const auto flags = os.flags();
  const auto width = os.width();
  const auto fill  = os.fill ();

  while (*c) {
    ::json_escape_char(os, *c);
    ++c;
  }

  os.flags(flags);
  os.width(width);
  os.fill (fill );

  return os;
}

std::ostream& cpl::util::json_escape(std::ostream& os, const std::string& s) {
  const auto flags = os.flags();
  const auto width = os.width();
  const auto fill  = os.fill ();

  for (const auto c: s) {
    ::json_escape_char(os, c);
  }

  os.flags(flags);
  os.width(width);
  os.fill (fill );

  return os;
}


////////////////////////////////////////////////////////////////////////
// UTF-8 stuff
////////////////////////////////////////////////////////////////////////

std::string cpl::util::utf8_tolower(std::string const& s) {
  auto ws = to_wstring(s);
  for (auto& c : ws) {
    c = std::tolower(c, utf8_locale);
  }
  return to_string(ws);
}

std::string cpl::util::utf8_toupper(std::string const& s) {
  auto ws = to_wstring(s);
  for (auto& c : ws) {
    c = std::toupper(c, utf8_locale);
  }
  return to_string(ws);
}

std::string cpl::util::utf8_canonical(
    std::string const& s,
    std::string const& extra,
    int const convert) {
  assert(-1 <= convert && convert <= 1);
  auto ws = to_wstring(s);
  std::wstring wret;
  for (auto c : ws) {
    char const cc = static_cast<char>(c);
    if (   std::isalnum(c, utf8_locale)
        // Check whether the wide char c has a char (i.e., ASCII) equivalent 
        // and whether that is in the extra set
        || (cc == c && std::string::npos != extra.find(cc))) {
      if        ( 1 == convert) {
        c = std::toupper(c, utf8_locale);
      } else if (-1 == convert) {
        c = std::tolower(c, utf8_locale);
      }
      wret.push_back(c);
    }
  }
  return to_string(wret);
}


////////////////////////////////////////////////////////////////////////
// End UTF-8 stuff; Begin String stuff
////////////////////////////////////////////////////////////////////////

bool cpl::util::is_trivial_string( std::string const& s )
{
  return s.empty() or (1 == s.size() and '-' == s[0]);
}

int cpl::util::update_if_nontrivial(std::string& s, std::string const& r)
{
  if (not cpl::util::is_trivial_string(r)) { 
    const int ret =
        cpl::util::is_trivial_string(s) ?
            2
          : (s == r ? 1 : -1);
    s = r; 
    return ret;
  } else {
    return 0;
  }
}


cpl::util::simple_scheduler::simple_scheduler( double const& dt_ )
: t_last( -std::numeric_limits< double >::max() )
{ reconfigure( dt_ ) ; }

void cpl::util::simple_scheduler::reconfigure( double const& dt_ )
{ always_assert( dt_ >= 0 ) ; dt = dt_ ; }


//
// File stuff.
//

using namespace cpl::util::file ;

// TODO: Better error reporting, factor out the common code
std::filebuf cpl::util::file::open_readbuf(
  std::string const& file                ,
  std::string      & which               ,
  std::vector< std::string > const& path
) {

  std::filebuf ret;

  std::string tried ;

  for( unsigned long i = 0 ; i < path.size() ; ++i ) {

    std::string const pathname = path[ i ] + "/" + file ;

    ret.open
      ( pathname.c_str() , std::ios_base::in | std::ios_base::binary ) ;

    if( !ret.is_open() )
    { tried += " " + pathname + ": " + cpl::detail_::get_strerror_message( errno ) ; }
    else { which = pathname ; return ret ; }

  }

  ret.open
    ( file.c_str() , std::ios_base::in | std::ios_base::binary ) ;

  if( !ret.is_open() )
  { tried += " " + file + ": " + cpl::detail_::get_strerror_message( errno ) ; }
  else { which = file ; return ret ; }

  assert( !ret.is_open() ) ;

  throw std::runtime_error
  ( "couldn't open " + file + " for reading:" + tried ) ;

}


std::filebuf
cpl::util::file::open_writebuf(
    std::string const& file ,
    std::ios_base::openmode const mode ) {

  std::filebuf ret;

  ret.open( file.c_str() , mode ) ;

  if( !ret.is_open() ) {

    throw std::runtime_error(
      "couldn't open " + file + " for writing: " + cpl::detail_::get_strerror_message( errno )
    ) ;

  }

  return ret ;

}


std::string cpl::util::file::basename(
  std::string const& name ,
  std::string const& suffix
) {

  if(
       name.size() >= suffix.size()
    && std::equal( name.end() - suffix.size() , name.end() , suffix.begin() )
  )
  { return std::string( name.begin() , name.end() - suffix.size() ) ; }
  else
  { return name ; }

}

cpl::util::file::logfile_manager::logfile_manager(
    long const keep_days_in ,
    std::string const& basename_in ,
    double const utc ,
    bool const remove_old ,
    long const bzip2_days_in)
  : basename( basename_in ) ,
    keep_days( keep_days_in ) ,
    bzip2_days( bzip2_days_in ) ,
    current_day( cpl::util::day_number( utc ) ) ,
    // Current day
    os(newfile()) {
  
  cpl::util::verify(keep_days >= 1, 
      "logfile_manager: keep_days must be >= 1");
  cpl::util::verify(bzip2_days < keep_days ,
      "logfile_manager: bzip2_days must 0 or < keep_days");

  if (remove_old) {
    // Remove files n to 2n days back: both bz2 and non-bz2
    for (int i = keep_days; i <= 2 * keep_days; ++i) {
      remove_old_files(i);
    }
  }
}

void cpl::util::file::logfile_manager::remove_old_files(const long i) {
  cpl::util::verify(i >= 1,
      "logfile_manager: WTF: Can only remove files at least 1 day old");
  auto const fn     = filename(current_day - i, false);
  auto const fn_bz2 = filename(current_day - i, true );
  bool const ignore_missing = true;
  cpl::util::file::unlink(fn    , ignore_missing);
  cpl::util::file::unlink(fn_bz2, ignore_missing);
}

cpl::util::file::owning_ofstream 
cpl::util::file::logfile_manager::newfile() {
  std::string const name = filename( current_day , false ) ;

  return cpl::util::file::open_write( name , 
      std::ios_base::binary | std::ios_base::out | std::ios_base::app ) ;
}

void cpl::util::file::logfile_manager::compress_file(const long i) const {
  cpl::util::verify(i >= 1,
      "logfile_manager: WTF: Can only compress files at least 1 day old");

  // bzip2 command in the background.  We don't care about the return
  // value.
  // See https://superuser.com/questions/178587/how-do-i-detach-a-process-from-terminal-entirely
  // Could use nohup as well.
  const auto cmd = "bzip2 -z " + filename(current_day - i, false) 
                 + " < /dev/null > /dev/null 2>&1 &";
  std::system(cmd.c_str());
}

bool cpl::util::file::logfile_manager::update( double const utc ) {
  long const new_day = cpl::util::day_number( utc ) ;

  if( new_day > current_day ) {
    current_day = new_day ;
    // This automatically closes the previous file
    os = newfile();
    if (bzip2_days >= 1) {
      compress_file(bzip2_days);
    }

    // bzip2_days < keep_days, so we won't remove the one currently
    // being zipped.
    remove_old_files(keep_days);
    return true ;
  } else {
    return false ;
  }
}

std::string cpl::util::file::logfile_manager::filename(
    long const day_number , bool const bz2 ) const {
  // Add some arbitrary number of seconds less than a day, to be more
  // comfortable with rounding
  auto ret = basename + "." 
           + cpl::util::format_date( day_number * cpl::units::day() + 12345.6) ;

  if ( bz2 ) {
    ret += ".bz2" ;
  }
  return ret;
}

void cpl::util::write_statistics_value(
    std::ostream& sl,
    const char* const name,
    const char* const description,
    long const total,
    long const counter) {
  boost::io::ios_precision_saver const s(sl);

  sl << log::prio::NOTICE
     << name
     << ": "
     << description
     << ": "
     << counter 
     << " ("
     << std::setprecision(3)
     << cpl::math::percentage(counter, total)
     << "%)"
     << std::endl;
}
