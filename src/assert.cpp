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

#include "nanonet/assert.h"

#include "nanonet/exception.h"


void nanonet::util::throw_timeout_exception(
    const double t, const std::string& op) {
  throw nanonet::util::timeout_exception(
      "Operation \"" + op
      + "\" timed out after "
      + std::to_string(t)
      + " second(s)");
}

void nanonet::util::throw_timeout_exception() {
  throw nanonet::util::timeout_exception("");
}

void nanonet::util::throw_shutdown_exception(
    const std::string& what) {
  throw nanonet::util::shutdown_exception(what);
}

void nanonet::util::throw_assertion_failure(
    const std::string& what) {
  throw nanonet::util::assertion_failure(what);
}

void nanonet::util::throw_runtime_error(
    const std::string& what) {
  throw std::runtime_error(what);
}
