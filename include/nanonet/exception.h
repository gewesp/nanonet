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
// Exception types extending the C++ standard exceptions:
//   http://en.cppreference.com/w/cpp/error/exception
//
// Low-level API.  Prefer the functions in error.h instead.
//


#ifndef NANONET_EXCEPTION_H
#define NANONET_EXCEPTION_H

#define NANONET_DETAIL_DECLARE_EXCEPTION(type, base)                  \
struct type : base {                                                  \
  explicit type(std::string const& what_arg) : base(what_arg) {}      \
  explicit type(const char* const  what_arg) : base(what_arg) {}      \
};

#include <stdexcept>
#include <string>

namespace nanonet {

namespace util {

// A parser detects a syntax or parse error, e.g. in registry
NANONET_DETAIL_DECLARE_EXCEPTION(parse_error, std::runtime_error)

// Assertion failure, e.g. from always_assert()
NANONET_DETAIL_DECLARE_EXCEPTION(assertion_failure, std::runtime_error)

// Timeout
NANONET_DETAIL_DECLARE_EXCEPTION(timeout_exception, std::runtime_error)

// Signal a (service) shutdown, exit etc.
NANONET_DETAIL_DECLARE_EXCEPTION(shutdown_exception, std::runtime_error)

} // namespace util

} // namespace nanonet


#endif // NANONET_EXCEPTION_H
