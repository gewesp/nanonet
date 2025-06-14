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
// Component: SYSUTIL
//
// File system operations
//
// This was created before the addition of the C++ filesystem library.
// Consider phasing out these functions in favor of the standard library
// (filesystem).
//


#ifndef CPP_LIB_FILE_SYS_H
#define CPP_LIB_FILE_SYS_H

#include <deque>
#include <memory>
#include <string>

#include "cpp-lib/util.h"
#include "cpp-lib/assert.h"



namespace nanonet {

namespace util {

namespace file {

// Utilities forwarding to the respective system functions with
// exceptions in case of errors.



} // namespace file

} // namespace util

} // namespace nanonet



#endif // CPP_LIB_FILE_SYS_H
