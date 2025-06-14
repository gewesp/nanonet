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

#include <iostream>

#include "nanonet/assert.h"
#include "nanonet/error.h"

// int main(int argc, const char* const* const argv) {
int main() {
  always_assert(4 == 2 + 2);

  std::cout << "The next assertion should fail:" << std::endl;
  try {
    // Next line is line 29, see the assert() below!
    always_assert(5 == 2 + 2);
  } catch (const nanonet::util::assertion_failure& e) {
    // Don't include path name b/c it is different on CI pipeline
    std::cout << "expected assertion failure was caught" << std::endl;
    always_assert(std::string(e.what()).find("Assertion failed: 5 == 2 + 2") == 0);
    always_assert(std::string(e.what()).find("error-test.cpp:29") != std::string::npos);
  }

  return 0;
}
