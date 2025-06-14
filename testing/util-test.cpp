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

#include "cpp-lib/container-util.h"
#include "cpp-lib/error.h"
#include "cpp-lib/random.h"
#include "cpp-lib/safe_queue.h"
#include "cpp-lib/type_traits.h"
#include "cpp-lib/util.h"
#include "cpp-lib/xdr.h"

#include "cpp-lib/sys/util.h"

using namespace nanonet::util            ;
using namespace nanonet::util::container ;

template<bool is_signed, int bits> void test_xdr(std::ostream& os) {
  os << "Testing marshalling, "
     << bits
     << " bits " 
     << (is_signed ? "" : "un")
     << "signed"
     << std::endl;

  typedef typename nanonet::xdr::integer<is_signed, bits>::type inttype;
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
    nanonet::xdr::write(p, v);
    auto const vv = nanonet::xdr::read_integer<is_signed, bits>(pp);
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
    nanonet::xdr::write(p, v);
    auto const vv = nanonet::xdr::read_raw<T>(pp);
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
      nanonet::xdr::read_float (it)
    : nanonet::xdr::read_double(it) ;
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
    std::string const s = nanonet::util::random_sequence<std::string>(
        mstd, sd, vd);

    char      * p  = &buf[0];
    char const* pp = &buf[0];
    nanonet::xdr::write(p, s);

    auto const ss = nanonet::xdr::read_string(pp, s.size());
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
  using int_is = nanonet::util::increment_sentry<int>;

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

template <typename T>
void is_container_test(const bool expected) {
  if (::nanonet::util::is_container<T>::value != expected) {
    ::nanonet::util::throw_error(
        std::string("is_container<> failed for ") + typeid(T).name());
  }
}

template <typename T>
void is_pair_test(const bool expected) {
  if (::nanonet::util::is_pair<T>::value != expected) {
    ::nanonet::util::throw_error(
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
  nanonet::util::chop(s);
  if (i == ' ' || i == '\t' || i == '\n' || i == '\v' || i == '\f' || i == '\r')
    always_assert(s.size() == n-2);
  else
    always_assert(s.size() == n-1);
}

void test_getline() {
  std::string s;
  while (nanonet::util::getline(std::cin, s, 10)) {
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
        q.push(std::to_string(i));
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
          long long const l = std::stoll(q.pop());
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
    std::string const s = nanonet::util::format_datetime(t);
    double const tt = nanonet::util::parse_datetime(s);
    double const ttt = nanonet::util::parse_datetime(s.substr(0, 10), "%F");
    std::cout << s << " " 
              << nanonet::util::format_date(tt) << " "
              << nanonet::util::format_time_no_z(tt) << " "
              << std::fmod(t, 60) 
              << std::endl;
    always_assert(std::fabs(t - tt) <= 1);
    always_assert(std::fabs(t - ttt) <= nanonet::units::day());
  }

  std::cout << format_date(r(mstd)) << std::endl;
  std::cout << format_time(r(mstd)) << std::endl;

  std::cout << format_datetime(-1) << std::endl;
  // Things still work, but get a bit imprecise 100
  // years back...
  std::cout << format_datetime(-100 * nanonet::units::year()) << std::endl;
  std::cout << format_datetime(-2000 * nanonet::units::year()) << std::endl;
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
  nanonet::util::capped_vector<int, 0> v0;
  nanonet::util::capped_vector<int, 5> v5;

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

using sv = std::vector<std::string>;
void test_split(std::ostream& os, std::string const& s, sv const& expected) {
  sv actual;
  nanonet::util::split(actual, s);
  if (actual != expected) {
    os << "split failed on: " << s << std::endl;
    os << "results:" << std::endl;
    for (auto const& el : actual) {
      os << '"' << el << '"' << std::endl;
    }
  }

  const sv actual2 = nanonet::util::split(s);
  if (actual2 != expected) {
    os << "splitter failed on: " << s << std::endl;
    os << "results:" << std::endl;
    for (auto const& el : actual2) {
      os << '"' << el << '"' << std::endl;
    }
  }

  nanonet::util::verify(actual  == expected, "Split failed: " + s);
  nanonet::util::verify(actual2 == expected, "Splitter failed: " + s);
}

void test_stringutils(std::ostream& os) {
  // tolower(), toupper()
  std::string h = "Hello World!";
  nanonet::util::toupper(h);
  nanonet::util::tolower(h);
  always_assert("hello world!" == h);

  // verify_alnum()
  nanonet::util::verify_alnum("");
  nanonet::util::verify_alnum("abc1234");
  nanonet::util::verify_alnum("abc1234+.", "+.,");
  nanonet::util::verify_alnum("abc1234\"+.", "\"+.,");
  nanonet::util::verify_alnum("abc1234\"+.", "\"+.,");

  // canonical()
  always_assert("" == nanonet::util::canonical(""));
  always_assert("" == nanonet::util::canonical("", "-+"));
  always_assert("HB1234" == nanonet::util::canonical("hB-12 34"));
  always_assert("HB1234" == nanonet::util::canonical("HB12.34"));
  always_assert("HB-1234" == nanonet::util::canonical("HB-12.34", "-"));

  test_split(os, "", sv{""});
  test_split(os, "1", sv{"1"});
  test_split(os, "1,2", sv{"1", "2"});
  test_split(os, "1, 2", sv{"1", " 2"});
  test_split(os, ", 2", sv{"", " 2"});
  test_split(os, ", ,", sv{"", " ", ""});

  verify_throws("invalid", nanonet::util::verify_alnum, "adsf+", "", true);
  verify_throws("invalid", nanonet::util::verify_alnum, "adsf+", "-", true);

  using namespace std::string_literals;
  always_assert(std::pair("k"s, "v"s) == nanonet::util::split_colon_blank("k: v"));
  always_assert(std::pair("k"s, "v"s) == nanonet::util::split_colon_blank("k:v"));
  always_assert(std::pair("k"s, "v"s) == nanonet::util::split_colon_blank("k:  v"));
  always_assert(std::pair("k"s, "v"s) == nanonet::util::split_colon_blank("k:     v"));
  always_assert(std::pair("k"s, "v "s) == nanonet::util::split_colon_blank("k:     v "));
  always_assert(std::pair("k "s, "v"s) == nanonet::util::split_colon_blank("k :v"));
  always_assert(std::pair(" k "s, "v"s) == nanonet::util::split_colon_blank(" k :v"));
  always_assert(std::pair(""s, "v"s) == nanonet::util::split_colon_blank(":v"));
  always_assert(std::pair(""s, ""s) == nanonet::util::split_colon_blank(":"));
  always_assert(std::pair(""s, ""s) == nanonet::util::split_colon_blank(":  "));
  always_assert(std::pair(" "s, ""s) == nanonet::util::split_colon_blank(" :  "));

  verify_throws("No colon found", nanonet::util::split_colon_blank, "k;v");
  verify_throws("No colon found", nanonet::util::split_colon_blank, "k v");
  verify_throws("No colon found", nanonet::util::split_colon_blank, "");

  {
    std::string s;
    always_assert(0 == nanonet::util::update_if_nontrivial(s, "-"));
    always_assert("" == s);
    always_assert(2 == nanonet::util::update_if_nontrivial(s, "s"));
    always_assert("s" == s);
    always_assert(0 == nanonet::util::update_if_nontrivial(s, "-"));
    always_assert("s" == s);
    always_assert(0 == nanonet::util::update_if_nontrivial(s, ""));
    always_assert("s" == s);
    always_assert(-1 == nanonet::util::update_if_nontrivial(s, "new_s"));
    always_assert("new_s" == s);
    always_assert(1 == nanonet::util::update_if_nontrivial(s, "new_s"));
    always_assert("new_s" == s);
    always_assert(-1 == nanonet::util::update_if_nontrivial(s, "even_newer_s"));
    always_assert("even_newer_s" == s);
  }
}

#if 0
void test_utf8_canonical() {
  always_assert(u8"" == nanonet::util::utf8_canonical(u8""));
  always_assert(u8"" == nanonet::util::utf8_canonical(u8"", "-+"));
  always_assert(u8"hÖ1234" == nanonet::util::utf8_canonical(u8"hÖ-12 34"));
  always_assert(u8"HB1234" == nanonet::util::utf8_canonical(u8"HB12.34"));

  // To upper, to lower
  always_assert(u8"HÜ-1234" ==
      nanonet::util::utf8_canonical(u8"Hü-12.34", "-", 1));
  always_assert(u8"hü-1234" ==
      nanonet::util::utf8_canonical(u8"Hü-12.34", "-", -1));

  always_assert(u8"CHÜCK YÄGER" == 
      nanonet::util::utf8_canonical(u8"chü.-\"'ck Yä&^,gEr...", " ", 1));

  always_assert(u8"fritz jäger-müller" ==
      nanonet::util::utf8_canonical(u8"Fritz. Jäger-+Müller", "- ", -1));
}

void test_utf8(std::ostream& os) {
  test_utf8_canonical();
  // std::u8string grussen = u8"grüßEN";
  // NOTE: As of 2025, using u8string will not work b/c there is no
  // iostream support for it...
  // It *did* work in C++17.
  std::string grussen = u8"grüßEN";

  os << "Original " << grussen   << std::endl;

  os << "Upper " << nanonet::util::utf8_toupper(grussen) << std::endl
     << "Lower " << nanonet::util::utf8_tolower(grussen) << std::endl
  ;

  os << "Upper: " << std::endl;
  os << nanonet::util::utf8_toupper("foO" ) << std::endl;
  os << nanonet::util::utf8_toupper("#foO") << std::endl;
  os << nanonet::util::utf8_toupper("ßfoO") << std::endl;
  os << nanonet::util::utf8_toupper("ÉfoO") << std::endl;
  os << nanonet::util::utf8_toupper("ÉfoO") << std::endl;

  os << "Lower: " << std::endl;
  os << nanonet::util::utf8_tolower("foO" ) << std::endl;
  os << nanonet::util::utf8_tolower("#foO") << std::endl;
  os << nanonet::util::utf8_tolower("ßfoO") << std::endl;
  os << nanonet::util::utf8_tolower("ÉfoO") << std::endl;
  os << nanonet::util::utf8_tolower("ÉfoO") << std::endl;
}
#endif

struct heartbeat_tester {
  heartbeat_tester(std::ostream& os) : os_(os) {}
  void heartbeat(const double& t) {
    os_ << "TIME " << std::setprecision(16) << t << std::endl;
  }
  std::ostream& os_;
};

void test_pacemaker(std::ostream& os) {
  heartbeat_tester tester(os);

  nanonet::util::pacemaker<heartbeat_tester> hb(tester, 1.0);
  nanonet::util::sleep(5);
  // Destructor should join the thread
}

int main(int argc, const char* const* const argv) {
  if (2 == argc && "pacemaker" == std::string(argv[1])) {
    test_pacemaker(std::cout);
    return 0;
  }

  test_datetime();
  test_format_time(nanonet::util::format_time_hh_mmt, std::cout);
  test_format_time(nanonet::util::format_time_hh_mm , std::cout);
  test_verify();
  test_capped_vector();

  test_getline();

  test_safe_queue_destructor();
  test_safe_queue(100000);

  test_type_traits();

  test_stringutils(std::cout);

  // test_utf8(std::cout);

  test_xdr(std::cout);

  test_increment_sentry();

        {
                int a1[20];
                int a2[1];
                int a3[0];
                always_assert(20 == nanonet::util::size(a1));
                always_assert(1  == nanonet::util::size(a2));
                always_assert(0  == nanonet::util::size(a3));
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
