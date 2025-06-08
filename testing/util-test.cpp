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

#include <iomanip>
#include <iostream>
#include <list>
#include <random>
#include <thread>
#include <type_traits>
#include <vector>

#include <cstdlib>

#include "cpp-lib/cgi.h"
#include "cpp-lib/container-util.h"
#include "cpp-lib/error.h"
#include "cpp-lib/random.h"
#include "cpp-lib/safe_queue.h"
#include "cpp-lib/type_traits.h"
#include "cpp-lib/util.h"
#include "cpp-lib/xdr.h"

#include "cpp-lib/sys/util.h"

using namespace cpl::util            ;
using namespace cpl::util::container ;

template<bool is_signed, int bits> void test_xdr(std::ostream& os) {
  os << "Testing marshalling, "
     << bits
     << " bits " 
     << (is_signed ? "" : "un")
     << "signed"
     << std::endl;

  typedef typename cpl::xdr::integer<is_signed, bits>::type inttype;
  static_assert(bits / 8 == sizeof(inttype),
      "integer size mismatch");

  double const max = 0.9 * std::pow(2, bits) * (is_signed ? 0.5 : 1.0);
  double const min = is_signed ? -max : 0.0;

  std::minstd_rand mstd;
  std::uniform_real_distribution<double> r(min, max); 

  char buf[sizeof(inttype)];

  for (int i = 0; i < 100000; ++i) {
    double const x = r(mstd);
    inttype const v = static_cast<inttype>(x);

    char* p = &buf[0];
    char const* pp = &buf[0];
    cpl::xdr::write(p, v);
    auto const vv = cpl::xdr::read_integer<is_signed, bits>(pp);
    always_assert( p == &buf[0] + bits / 8);
    always_assert(pp == &buf[0] + bits / 8);
    if (v != vv) {
      os << v << " " << vv << std::endl;
    }
    always_assert(vv == v);
  }
}

template<typename T>
void test_xdr_float(std::ostream& os) {
  int constexpr bits = 8 * sizeof(T);
  os << "Testing float marshalling, "
     << bits
     << " bits" 
     << std::endl;

  double const max = 1e-3 * std::numeric_limits<T>::max();

  std::minstd_rand mstd;
  std::uniform_real_distribution<double> r(-max, max); 

  char buf[sizeof(T)];

  for (int i = 0; i < 100000; ++i) {
    double const x = r(mstd);
    T const v = static_cast<T>(x);

    char* p = &buf[0];
    char const* pp = &buf[0];
    cpl::xdr::write(p, v);
    auto const vv = cpl::xdr::read_raw<T>(pp);
    always_assert(p  == &buf[0] + bits / 8);
    always_assert(pp == &buf[0] + bits / 8);
    if (v != vv) {
      os << v << " " << vv << std::endl;
    }
    always_assert(vv == v);
  }
}

void test_xdr_float_ieee(
    std::ostream& os, std::vector<unsigned char> const& hex, double const x) {
  auto it = hex.begin();
  double const decoded = hex.size() == 4 ? 
      cpl::xdr::read_float (it)
    : cpl::xdr::read_double(it) ;
  always_assert(hex.end() == it);

  double const diff = std::fabs(decoded / x - 1);

  if (4 == hex.size()) { 
    if (diff >= 1e-7) {
      os.precision(16);
      os << "orig: " << x << ", decoded: " << decoded << std::endl;
    }
    always_assert(std::fabs(decoded / x - 1) < 1e-7);
  } else {
    if (diff >= 1e-15) {
      os.precision(16);
      os << "orig: " << x << ", decoded: " << decoded << std::endl;
    }
    always_assert(std::fabs(decoded / x - 1) < 1e-15);
  }
}

// http://www.h-schmidt.net/FloatConverter/IEEE754.html
// http://babbage.cs.qc.cuny.edu/IEEE-754.old/Decimal.html
void test_xdr_float_ieee(std::ostream& os) {
  os << "Testing float marshalling IEEE" << std::endl;

  test_xdr_float_ieee(os, { 0x10, 0x06, 0x9e, 0x3f }, 1.23456);
  test_xdr_float_ieee(os, { 0x6f, 0x12, 0x83, 0x3a }, 1e-3   );
  test_xdr_float_ieee(os, { 0x52, 0xea, 0x64, 0x72 }, 4.534135e30);

  test_xdr_float_ieee(os, { 0xa4, 0x70, 0x3c, 0xc2 }, -47.11);

  test_xdr_float_ieee(os,
      { 0xA7, 0x04, 0xC0, 0x98, 0x1F, 0x96, 0x91, 0x7E },
      4.711e301);
}

void test_xdr_string(std::ostream& os) {
  os << "Testing string marshalling" << std::endl;

  std::minstd_rand mstd;

  // Size distribution of random strings: 0 to 300
  std::uniform_int_distribution<int> sd(0, 300);
  // Values: a-z
  std::uniform_int_distribution<char> vd('a', 'z');

  char buf[400];

  for (int i = 0; i < 100000; ++i) {
    std::string const s = cpl::util::random_sequence<std::string>(
        mstd, sd, vd);

    char      * p  = &buf[0];
    char const* pp = &buf[0];
    cpl::xdr::write(p, s);

    auto const ss = cpl::xdr::read_string(pp, s.size());
    always_assert(pp == p);
    always_assert((p - &buf[0]) % 4 == 0);
    always_assert(static_cast<long>(s.size()) <= p - &buf[0]);
    always_assert(p - &buf[0] <= static_cast<long>(s.size()) + 3);
    always_assert(ss == s);
  }
}

void test_xdr(std::ostream& os) {
  test_xdr<false, 16>(os);
  test_xdr<false, 32>(os);
  test_xdr<false, 64>(os);

  test_xdr<true, 16>(os);
  test_xdr<true, 32>(os);
  test_xdr<true, 64>(os);

  test_xdr_float<float >(os);
  test_xdr_float<double>(os);

  test_xdr_float_ieee(os);

  test_xdr_string(os);
}

void test_increment_sentry() {
  using int_is = cpl::util::increment_sentry<int>;

  int x = 0;

  {
    int_is sen1(x);
    always_assert(1 == x);

    {
      int_is sen2(x);
      always_assert(2 == x);
    }
    always_assert(1 == x);
  }
  always_assert(0 == x);

  int_is sen1(x);

  always_assert(1 == x);

  auto sen2 = std::move(sen1);
  always_assert(1 == x);

  std::vector<int_is> vec;
  always_assert(1 == x);

  vec.push_back(int_is(x));
  vec.push_back(int_is(x));
  vec.push_back(int_is(x));

  always_assert(4 == x);
  vec.clear();
  always_assert(1 == x);
}

void test_output(std::ostream& os) {
  const std::vector<int> test({47, 11, 1, 2, 3});
  cpl::util::write_list(os, test.begin(), test.end());
  os << std::endl;

  cpl::util::write_statistics_value(
      os, "Test statistics", "Half of elements", 100, 50);
}

template <typename T>
void is_container_test(const bool expected) {
  if (::cpl::util::is_container<T>::value != expected) {
    ::cpl::util::throw_error(
        std::string("is_container<> failed for ") + typeid(T).name());
  }
}

template <typename T>
void is_pair_test(const bool expected) {
  if (::cpl::util::is_pair<T>::value != expected) {
    ::cpl::util::throw_error(
        std::string("is_pair<> failed for ") + typeid(T).name());
  }
}

void test_type_traits() {
  is_container_test<int>(false);
  is_container_test<double>(false);
  is_container_test<void>(false);
  is_container_test<std::pair<int, int>>(false);

  is_container_test<std::vector<int>>(true);
  is_container_test<std::map<int, double>>(true);
  is_container_test<std::map<int, double>>(true);

  is_pair_test<int>(false);
  is_pair_test<double>(false);
  is_pair_test<void>(false);
  is_pair_test<std::vector<int>>(false);
  is_pair_test<std::map<int, double>>(false);

  is_pair_test<std::pair<int, int>>(true);
  is_pair_test<std::pair<int*, double>>(true);
}

void testChop(int i) {
  std::string s("...X ");
  std::size_t n = s.size();
  s[3] = i;
  cpl::util::chop(s);
  if (i == ' ' || i == '\t' || i == '\n' || i == '\v' || i == '\f' || i == '\r')
    always_assert(s.size() == n-2);
  else
    always_assert(s.size() == n-1);
}

void test_cgi(std::ostream& os, std::string const& query) {
  os << "CGI query: " << query << std::endl;
  try {
    auto const keyval = cpl::cgi::parse_query(query);
    for (auto const& kv : keyval) {
      os << kv.first << " = " << kv.second << std::endl;
    }
  } catch (std::exception const& e) {
    os << "ERROR: " << e.what() << std::endl;
  }
}

void test_cgi(std::ostream& os) {
  os << "CGI parameter parsing" << std::endl;

  test_cgi(os, "bounds_sw=46.949049%2C7.887476&bounds_ne=47.050902%2C8.112524");
  test_cgi(os, "x=y");
  test_cgi(os, "x=y&a=b");
  test_cgi(os, "x=1.234y&a=b");
  test_cgi(os, "x=1.234y");
  test_cgi(os, "1=2");
  test_cgi(os, "q=2");

  // OK: "" = ""
  test_cgi(os, "=");
  // OK: "" = y
  test_cgi(os, "=y");
  // OK: a = "", "" = ""
  test_cgi(os, "a=&=");

  // OK: Empty query
  test_cgi(os, "");
  test_cgi(os, " ");

  // OK: Whitespace trimmed 
  test_cgi(os, " x  =     0815 ");
  test_cgi(os, " x  =     0815  & y    = 4711");

  // OK: Whitespace
  test_cgi(os, " x  =     0815 ");

  // OK: Escaped whitespace
  test_cgi(os, "Param%20with%20whitespace = foo%20bar");

  os << "The following should FAIL:" << std::endl;
  test_cgi(os, "&abc&x=1.234");
  test_cgi(os, "y");
  test_cgi(os, "&=");
  test_cgi(os, "=&");
  test_cgi(os, "&");
  test_cgi(os, "&   & &&");
  test_cgi(os, "& ");
  test_cgi(os, " &");
  test_cgi(os, "x=1.234y&");

  // OK: Plus -> whitespace
  test_cgi(os, "x=hello+world");
  test_cgi(os, "foo+bar=hello+++world");
  test_cgi(os, "foo+bar=+++world");
  test_cgi(os, "foo+bar=+++");

  // Duplicates
  test_cgi(os, "=&=");
  test_cgi(os, "a=&a=b");
}

void test_getline() {
  std::string s;
  while (cpl::util::getline(std::cin, s, 10)) {
    std::cout << "string size: " << s.size() << "; string: " << s << std::endl;
  }
}

void test_safe_queue_destructor() {
  safe_queue<std::string> q;

  std::thread reader(
    [&q]{
      q.pop();
  });

  q.push("hello world");

  // Must join() here, otherwise the queue may get destructed before the 
  // reader accesses it.
  reader.join();
}

void test_safe_queue(long long const n) {
  std::cout << "Testing safe_queue" << std::endl;
  safe_queue<std::string> q;
  
  // Producer
  std::thread t1{
    [n, &q]{
      for (long i = 0; i < n; ++i) {
        q.push(boost::lexical_cast<std::string>(i));
        // std::cout << "push " << i << std::endl;
      }
    }
  };

  long long sum = 0;
  // Consumer
  std::thread t2{
      [n, &q, &sum] {
        long long prev = -10;

        for (long i = 0; i < n; ++i) {
          long long const l = boost::lexical_cast<long long>(q.pop());
          // std::cerr << "pop " << l << std::endl;
          if (prev >= 0) {
            always_assert(prev + 1 == l);
          }
          prev = l;
          sum += l;
        }  
      }
  };

  t1.join();
  t2.join();

  always_assert(n * (n - 1) / 2 == sum);
  std::cout << "OK" << std::endl;
}

template< typename C >
void check_ascending() {

  C l ;
  l.clear() ;
  l.push_back( 0  ) ;
  l.push_back( 1  ) ;
  l.push_back( -1 ) ;

  bool ok = false ;

  try { check_strictly_ascending( l.begin() , l.end() ) ; }
  catch( std::exception const& e ) {

        ok = true ;
        std::cout << "check_strictly_ascending() works: " << e.what() << '\n' ;

  }

  if( !ok ) {

        std::cout << "check_strictly_ascending() failed\n" ;

  }


}

template< typename C >
void check_iterator() {

  typedef typename C::const_iterator ci ;

  std::cout << "iterator advance:\n" ;

  C l( 10 ) ;

  ci i = l.begin() ;

  while( ( i = safe_advanced( i , ci( l.end() ) , 2 ) ) != l.end() ) 
  { std::cout << std::distance( ci( l.begin() ) , i ) << '\n' ; }

  check_ascending< C >() ;

}

void test_datetime() {
  std::minstd_rand mstd;
  std::uniform_real_distribution<double> r(0, 1e9); 

  for (int i = 0; i < 100; ++i) {
    double const t = r(mstd);
    std::string const s = cpl::util::format_datetime(t);
    double const tt = cpl::util::parse_datetime(s);
    double const ttt = cpl::util::parse_datetime(s.substr(0, 10), "%F");
    std::cout << s << " " 
              << cpl::util::format_date(tt) << " "
              << cpl::util::format_time_no_z(tt) << " "
              << std::fmod(t, 60) 
              << std::endl;
    always_assert(std::fabs(t - tt) <= 1);
    always_assert(std::fabs(t - ttt) <= cpl::units::day());
  }

  std::cout << format_date(r(mstd)) << std::endl;
  std::cout << format_time(r(mstd)) << std::endl;

  std::cout << format_datetime(-1) << std::endl;
  // Things still work, but get a bit imprecise 100
  // years back...
  std::cout << format_datetime(-100 * cpl::units::year()) << std::endl;
  std::cout << format_datetime(-2000 * cpl::units::year()) << std::endl;
  std::cout << format_datetime(-100, default_datetime_format(), 0, "too old")
            << std::endl;
}

template<typename F>
void test_format_time(F const& f, std::ostream& os) {
  os << f(3600, true)       << std::endl;
  os << f(3599.999, true)   << std::endl;
  os << f(0, true)       << std::endl;
  os << f(10.1, true)    << std::endl;
  os << f(6, true)       << std::endl;
  os << f(60, true)      << std::endl;
  os << f(4711, true)    << std::endl;

  os << f(123.34, true)  << std::endl;
  os << f(3 * 3600 + 1234, true) << std::endl;
  os << f(11 * 3600 + 353, true) << std::endl;

  os << f(23  , false) << std::endl;
  os << f(7.1 , false) << std::endl;
}


void test_verify() {
  verify(0 <= 1, "This shouldn't throw...");
  try {
    verify(std::sin(1234) > 10, "verify error");
  } catch (std::runtime_error const& e) {
    always_assert(std::string(e.what()) == "verify error");
  }
}

void test_capped_vector() {
  cpl::util::capped_vector<int, 0> v0;
  cpl::util::capped_vector<int, 5> v5;

  always_assert(0 == v0.size());
  always_assert(0 == v5.size());

  always_assert(0 == v0.capacity());
  always_assert(5 == v5.capacity());

  always_assert(v0.empty());
  always_assert(v5.empty());

  always_assert(!v5.full());

  v5.push_back(1);
  always_assert(1 == v5.front());
  always_assert(!v5.empty());
  v5.push_back(2);
  always_assert(1 == v5.front());
  always_assert(!v5.full());

  v5.push_back(3);
  v5.push_back(4);
  v5.push_back(5);
  always_assert(5 == v5.size());
  always_assert(1 == v5.front());
  always_assert(v5.full());
  v5.pop_back();
  always_assert(!v5.full());
}

void test_uri(std::ostream& os, std::string const& uri) {
  os << uri << " -> " << cpl::cgi::uri_decode(uri) << std::endl;
}

void test_uri_throws(
    std::ostream& os,
    std::string const& uri, std::string const& whattext) {
  os << uri << " should trigger exception with: " << whattext << std::endl;
  verify_throws(whattext, 
                cpl::cgi::uri_decode, uri, true);
}

void test_uri(std::ostream& os) {
  test_uri_throws(os, "ege%", "syntax");
  test_uri_throws(os, "ege%2", "syntax");
  test_uri_throws(os, "ege%%", "syntax");
  test_uri_throws(os, "%33%%", "syntax");

  test_uri_throws(os, "ege%2 ", "hex");
  test_uri_throws(os, "ege%%1", "hex");
  test_uri_throws(os, "ege%% 123", "hex");

  test_uri(os, "demo%3amain");
  test_uri(os, "demo%3Amain");
  test_uri(os, "demo%3A%50ain");
  test_uri(os, "%3A%50");
  test_uri(os, "%3a%50");
}

using sv = std::vector<std::string>;
void test_split(std::ostream& os, std::string const& s, sv const& expected) {
  sv actual;
  cpl::util::split(actual, s);
  if (actual != expected) {
    os << "split failed on: " << s << std::endl;
    os << "results:" << std::endl;
    for (auto const& el : actual) {
      os << '"' << el << '"' << std::endl;
    }
  }

  const sv actual2 = cpl::util::split(s);
  if (actual2 != expected) {
    os << "splitter failed on: " << s << std::endl;
    os << "results:" << std::endl;
    for (auto const& el : actual2) {
      os << '"' << el << '"' << std::endl;
    }
  }

  cpl::util::verify(actual  == expected, "Split failed: " + s);
  cpl::util::verify(actual2 == expected, "Splitter failed: " + s);
}

void test_stringutils(std::ostream& os) {
  // tolower(), toupper()
  std::string h = "Hello World!";
  cpl::util::toupper(h);
  cpl::util::tolower(h);
  always_assert("hello world!" == h);

  // verify_alnum()
  cpl::util::verify_alnum("");
  cpl::util::verify_alnum("abc1234");
  cpl::util::verify_alnum("abc1234+.", "+.,");
  cpl::util::verify_alnum("abc1234\"+.", "\"+.,");
  cpl::util::verify_alnum("abc1234\"+.", "\"+.,");

  // canonical()
  always_assert("" == cpl::util::canonical(""));
  always_assert("" == cpl::util::canonical("", "-+"));
  always_assert("HB1234" == cpl::util::canonical("hB-12 34"));
  always_assert("HB1234" == cpl::util::canonical("HB12.34"));
  always_assert("HB-1234" == cpl::util::canonical("HB-12.34", "-"));

  test_split(os, "", sv{""});
  test_split(os, "1", sv{"1"});
  test_split(os, "1,2", sv{"1", "2"});
  test_split(os, "1, 2", sv{"1", " 2"});
  test_split(os, ", 2", sv{"", " 2"});
  test_split(os, ", ,", sv{"", " ", ""});

  verify_throws("invalid", cpl::util::verify_alnum, "adsf+", "", true);
  verify_throws("invalid", cpl::util::verify_alnum, "adsf+", "-", true);

  using namespace std::string_literals;
  always_assert(std::pair("k"s, "v"s) == cpl::util::split_colon_blank("k: v"));
  always_assert(std::pair("k"s, "v"s) == cpl::util::split_colon_blank("k:v"));
  always_assert(std::pair("k"s, "v"s) == cpl::util::split_colon_blank("k:  v"));
  always_assert(std::pair("k"s, "v"s) == cpl::util::split_colon_blank("k:     v"));
  always_assert(std::pair("k"s, "v "s) == cpl::util::split_colon_blank("k:     v "));
  always_assert(std::pair("k "s, "v"s) == cpl::util::split_colon_blank("k :v"));
  always_assert(std::pair(" k "s, "v"s) == cpl::util::split_colon_blank(" k :v"));
  always_assert(std::pair(""s, "v"s) == cpl::util::split_colon_blank(":v"));
  always_assert(std::pair(""s, ""s) == cpl::util::split_colon_blank(":"));
  always_assert(std::pair(""s, ""s) == cpl::util::split_colon_blank(":  "));
  always_assert(std::pair(" "s, ""s) == cpl::util::split_colon_blank(" :  "));

  verify_throws("No colon found", cpl::util::split_colon_blank, "k;v");
  verify_throws("No colon found", cpl::util::split_colon_blank, "k v");
  verify_throws("No colon found", cpl::util::split_colon_blank, "");

  {
    std::string s;
    always_assert(0 == cpl::util::update_if_nontrivial(s, "-"));
    always_assert("" == s);
    always_assert(2 == cpl::util::update_if_nontrivial(s, "s"));
    always_assert("s" == s);
    always_assert(0 == cpl::util::update_if_nontrivial(s, "-"));
    always_assert("s" == s);
    always_assert(0 == cpl::util::update_if_nontrivial(s, ""));
    always_assert("s" == s);
    always_assert(-1 == cpl::util::update_if_nontrivial(s, "new_s"));
    always_assert("new_s" == s);
    always_assert(1 == cpl::util::update_if_nontrivial(s, "new_s"));
    always_assert("new_s" == s);
    always_assert(-1 == cpl::util::update_if_nontrivial(s, "even_newer_s"));
    always_assert("even_newer_s" == s);
  }
}

void test_utf8_canonical() {
  always_assert(u8"" == cpl::util::utf8_canonical(u8""));
  always_assert(u8"" == cpl::util::utf8_canonical(u8"", "-+"));
  always_assert(u8"hÖ1234" == cpl::util::utf8_canonical(u8"hÖ-12 34"));
  always_assert(u8"HB1234" == cpl::util::utf8_canonical(u8"HB12.34"));

  // To upper, to lower
  always_assert(u8"HÜ-1234" ==
      cpl::util::utf8_canonical(u8"Hü-12.34", "-", 1));
  always_assert(u8"hü-1234" ==
      cpl::util::utf8_canonical(u8"Hü-12.34", "-", -1));

  always_assert(u8"CHÜCK YÄGER" == 
      cpl::util::utf8_canonical(u8"chü.-\"'ck Yä&^,gEr...", " ", 1));

  always_assert(u8"fritz jäger-müller" ==
      cpl::util::utf8_canonical(u8"Fritz. Jäger-+Müller", "- ", -1));
}

void test_utf8(std::ostream& os) {
  test_utf8_canonical();
  std::string grussen = u8"grüßEN";

  os << "Original " << grussen   << std::endl;

  os << "Upper " << cpl::util::utf8_toupper(grussen) << std::endl
     << "Lower " << cpl::util::utf8_tolower(grussen) << std::endl
  ;

  os << "Upper: " << std::endl;
  os << cpl::util::utf8_toupper("foO" ) << std::endl;
  os << cpl::util::utf8_toupper("#foO") << std::endl;
  os << cpl::util::utf8_toupper("ßfoO") << std::endl;
  os << cpl::util::utf8_toupper("ÉfoO") << std::endl;
  os << cpl::util::utf8_toupper("ÉfoO") << std::endl;

  os << "Lower: " << std::endl;
  os << cpl::util::utf8_tolower("foO" ) << std::endl;
  os << cpl::util::utf8_tolower("#foO") << std::endl;
  os << cpl::util::utf8_tolower("ßfoO") << std::endl;
  os << cpl::util::utf8_tolower("ÉfoO") << std::endl;
  os << cpl::util::utf8_tolower("ÉfoO") << std::endl;
}

struct heartbeat_tester {
  heartbeat_tester(std::ostream& os) : os_(os) {}
  void heartbeat(const double& t) {
    os_ << "TIME " << std::setprecision(16) << t << std::endl;
  }
  std::ostream& os_;
};

void test_pacemaker(std::ostream& os) {
  heartbeat_tester tester(os);

  cpl::util::pacemaker<heartbeat_tester> hb(tester, 1.0);
  cpl::util::sleep(5);
  // Destructor should join the thread
}

int main(int argc, const char* const* const argv) {
  if (2 == argc && "pacemaker" == std::string(argv[1])) {
    test_pacemaker(std::cout);
    return 0;
  }

  test_datetime();
  test_format_time(cpl::util::format_time_hh_mmt, std::cout);
  test_format_time(cpl::util::format_time_hh_mm , std::cout);
  test_verify();
  test_capped_vector();

  test_getline();

  test_safe_queue_destructor();
  test_safe_queue(100000);

  test_type_traits();

  test_cgi(std::cout);

  test_stringutils(std::cout);

  test_utf8(std::cout);

  test_uri(std::cout);

  test_xdr(std::cout);

  test_increment_sentry();

  test_output(std::cout);

        {
                int a1[20];
                int a2[1];
                int a3[0];
                always_assert(20 == cpl::util::size(a1));
                always_assert(1  == cpl::util::size(a2));
                always_assert(0  == cpl::util::size(a3));
        }

        {
                for (int i=1; i < 256; ++i)
                        testChop(i);
        }

        std::cout << "check_iterator< std::list  < int > >()\n" ;
        check_iterator< std::list  < int > >() ;

        std::cout << "check_iterator< std::vector< int > >()\n" ;
        check_iterator< std::vector< int > >() ;

        always_assert(2+2==4);
        return 0;
}
