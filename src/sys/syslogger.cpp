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
// TODO: A variant of that for windows, or put it in the POSIX component.
//

#include "cpp-lib/sys/syslogger.h"

#include <array>
#include <iostream>
#include <cassert>

#include <syslog.h>

#include "cpp-lib/util.h"


using namespace nanonet::util::log ;

namespace {

// Initializer list: Use double curly braces
// https://stackoverflow.com/questions/22501368/why-wasnt-a-double-curly-braces-syntax-preferred-for-constructors-taking-a-std
std::array<const char*, 8> prionames = {{
  "EMERGENCY" ,
  "ALERT"     ,
  "CRITICAL"  ,
  "ERROR"     ,
  "WARNING"   ,
  "NOTICE"    ,
  "INFO"      ,
  "DEBUG"    
}};

} // anonymous namespace

const char* nanonet::util::log::to_string(nanonet::util::log::prio const p) {
  assert(nanonet::util::log::prio::EMERG <= p);
  assert(                          p <= nanonet::util::log::prio::DEBUG);
  return prionames[static_cast<int>(p)];
}

nanonet::util::log::prio nanonet::util::log::prio_from_string(std::string const& s) {
  for (unsigned i = 0; i < prionames.size(); ++i) {
    if (s == prionames.at(i)) {
      return static_cast<nanonet::util::log::prio>(i);
    }
  }

  throw std::runtime_error("Syslogger: Unknown logging priority: " + s);
}

// Set log priority for next message
std::ostream& nanonet::util::log::operator<<(
    std::ostream& os, nanonet::util::log::prio const p) {
  auto const ptr = dynamic_cast<syslogger*>(&os);
 
  // For non-sysloggers, we write a textual representation
  if (ptr) { 
    ptr->buffer().reader_writer().currlevel = p;
  } else {
    os << '(' << to_string(p) << ") ";
  }
  return os;
}

// No-op for non-sysloggers
std::ostream& nanonet::util::log::operator<<(
    std::ostream& os, nanonet::detail_::prio_setter const& ps) {
  auto const ptr = dynamic_cast<syslogger*>(&os);
  if (ptr) {
    ptr->set_minlevel(ps);
  }
  return os;
}
 
nanonet::detail_::syslog_writer::syslog_writer(
    std::string const& tag,
    std::ostream* const echo)
  : minlevel_syslog(::nanonet::util::log::default_prio()),
    minlevel_echo  (::nanonet::util::log::default_prio()),
    currlevel      (::nanonet::util::log::default_prio()),
    echo_(echo) {
  // If we have a tag, make it "<tag> ", else ""
  if (tag.size() > 0) {
    tag_.insert(tag_.end(), tag.begin(), tag.end());
    tag_.push_back(' ');
  }

  // Ensure termination, this must be a C string!
  tag_.push_back(0);
}
 

int nanonet::detail_::syslog_writer::write(char const* const buf, int const n) {
  int last = n;
  // Trim whitespace at end of line, typically from use of
  // std::endl
  while (last >= 1 && std::isspace(buf[last - 1])) {
    --last;
  }

  assert(last >= 0);

  if (currlevel <= minlevel_syslog) {
    syslog(LOG_EMERG + static_cast<int>(currlevel),
           "%s(%s) %.*s", tag(), to_string(currlevel), last, buf);
  }

  if (echo_ && currlevel <= minlevel_echo) {
    // Time values < 0 disable the echo clock
    const double now = echo_clock_();
    if (now >= 0) {
      *echo_ << nanonet::util::format_datetime(echo_clock_()) << ' ';
    }

    *echo_ << tag() << '(' << to_string(currlevel) << ") ";
    std::copy(buf, buf + last, std::ostreambuf_iterator<char>(*echo_));
    *echo_ << std::endl;
  }

  // Reset log level
  currlevel = default_prio();

  // Pretend we wrote OK
  return n;
}

void nanonet::util::log::log_error(
    std::ostream& os,
    std::string const& msg,
    std::string const& what) {
  os << prio::ERR << msg << ": " << what << std::endl;
}

void nanonet::util::log::log_oneoff(
    std::string const& tag,
    const prio p,
    const std::string& message) {
  syslogger sl(tag);
  sl << p << message << std::endl;
}
