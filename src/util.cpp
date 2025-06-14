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

#include "nanonet/util.h"


#include "nanonet/error.h"
#include "nanonet/exception.h"

#include "nanonet/detail/platform_wrappers.h"
#include "nanonet/sys/syslogger.h"

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


using namespace nanonet::util ;


namespace {



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

} // anonymous namespace



// TODO: Templatize, etc?
// http://en.cppreference.com/w/cpp/string/basic_string/getline
std::istream& nanonet::util::getline(
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

std::string nanonet::util::trim(const std::string& s) {
  auto ret = s;
  ret.erase(0, ret.find_first_not_of(" \n\r\t"));
  ret.erase(ret.find_last_not_of(" \n\r\t") + 1);
  return s;
}

std::vector<std::string> nanonet::util::split(std::string const& str, char const sep) {
  std::string back;
  nanonet::util::splitter s(str, sep);

  std::vector<std::string> ret;
  while (s.get_next(back)) {
    ret.push_back(back);
  }

  return ret;
}

std::pair<std::string, std::string> nanonet::util::split_pair(
    std::string const& s,
    const char* const separators) {
  std::vector<std::string> v;
  nanonet::util::split(v, s, separators);
  nanonet::util::verify(2 == v.size(),
      "split_pair(): Expected 2 fields, got " + std::to_string(v.size()));
  return std::make_pair(v[0], v[1]);
}

std::pair<std::string, std::string> nanonet::util::split_colon_blank(
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

bool nanonet::util::is_stdin(const std::string& f) {
  return "-" == f or "stdin" == f or "STDIN" == f;
}

bool nanonet::util::is_stdout(const std::string& f) {
  return "-" == f or "stdout" == f or "STDOUT" == f;
}

void nanonet::util::assertion(
  const bool expr ,
  std::string const& expr_string ,
  std::string const& file ,
  const long line
) {
  // expr is true -> All OK
  if (expr) {
    return;
  }

  nanonet::util::throw_assertion_failure(
        "Assertion failed: " 
      + expr_string 
      + " (" + file + ":" + std::to_string(line) + ")");
}

void nanonet::util::verify(
  const bool expression ,
  std::string const& message
) { 
  if( !expression ) { throw std::runtime_error( message ) ; }
}


long nanonet::util::check_long
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


double nanonet::util::utc() {
  return 
    std::chrono::duration<double>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

std::tm nanonet::util::utc_tm(double const t) {
  std::time_t const t1 = t + 0.5;
  std::tm ret;
  // Better safe than sorry: Set members to 0
  nanonet::util::clear(ret);
  if (NULL == ::gmtime_r(&t1, &ret)) {
    nanonet::detail_::strerror_exception("gmtime()", errno);
  }
  return ret;
}

std::string nanonet::util::format_datetime(
    double const t, char const* const format,
    double const past_limit, char const* const default_result) {
  if (t < past_limit) {
    return default_result;
  }

  std::tm t2 = nanonet::util::utc_tm(t);

  char ret[100];
  if (0 == strftime(ret, 99, format, &t2)) {
    throw std::runtime_error("format_datetime(): formatting failure");
  }
  return ret;
}

std::string nanonet::util::format_time_hh_mm(double const& dt,
                                         bool const skip_hour) {
  assert( dt >= 0 ) ;

  double h = std::floor(dt / nanonet::units::hour());
  double m = std::round((dt - h * nanonet::units::hour()) / nanonet::units::minute());
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


std::string nanonet::util::format_time_hh_mmt(double const& dt,
                                          bool const skip_hour) {
    
  assert( dt >= 0 ) ;

  double h = std::floor(dt / nanonet::units::hour());
  // Round to nearest 10th of minute
  double m = 0.1 * std::round(
      10 * (dt - h * nanonet::units::hour()) / nanonet::units::minute());
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

double nanonet::util::parse_datetime(
    std::string const& s, char const* format) {
  std::tm t2;

  // Set members to 0.  Absolutely necessary here since strptime()
  // may leave fields untouched, depending on the format.
  nanonet::util::clear(t2);

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


// The cast is necessary because std::isspace(int) is undefined for
// value other than EOF or unsigned char

void nanonet::util::chop( std::string& s ) {

  std::size_t i ;
  for(
    i = s.size() ;
    i > 0 && std::isspace( static_cast< unsigned char >( s[ i - 1 ] ) ) ;
    --i
  )
  {}

  s.resize( i ) ;

}

std::string nanonet::util::canonical(
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

////////////////////////////////////////////////////////////////////////
// UTF-8 stuff
////////////////////////////////////////////////////////////////////////

std::string nanonet::util::utf8_tolower(std::string const& s) {
  auto ws = to_wstring(s);
  for (auto& c : ws) {
    c = std::tolower(c, utf8_locale);
  }
  return to_string(ws);
}

std::string nanonet::util::utf8_toupper(std::string const& s) {
  auto ws = to_wstring(s);
  for (auto& c : ws) {
    c = std::toupper(c, utf8_locale);
  }
  return to_string(ws);
}

std::string nanonet::util::utf8_canonical(
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
