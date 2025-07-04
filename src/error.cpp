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

#include "nanonet/error.h"

#include "nanonet/exception.h"

#include <string>

void nanonet::util::throw_error(const char* const message) {
  throw std::runtime_error(message);
}

void nanonet::util::throw_error(const std::string& message) {
  throw std::runtime_error(message);
}

void nanonet::util::throw_parse_error(const char* const message) {
  nanonet::util::throw_parse_error(std::string("Parse error: ") + message);
}

void nanonet::util::throw_parse_error(const std::string& message) {
  throw nanonet::util::parse_error("Parse error: " + message);
}
