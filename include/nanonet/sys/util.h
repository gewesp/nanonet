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

//
// Header for functionality that has an OS-independent interface but
// needs to be implemented in an OS specific way.
//
// Includes:
//   time(), sleep()
//   reboot(), poweroff()
//   sleep_scheduler
//   watched_registry
//


#ifndef CPP_LIB_UTIL_SYS_H
#define CPP_LIB_UTIL_SYS_H

#include "nanonet/util.h"

#include <atomic>
#include <thread>

namespace nanonet {

namespace util {

//
// Sleep for the specified time [s].  Accuracy is system-dependent,
// typically in the range of 10 milliseconds.
//

void sleep( double const& ) ;

//
// Returns the time [s] since some fixed or variable epoch.  On
// Unix, the epoch is January 1st, 1970.  It may also be
// system startup, however.
//
// Accuracy is system-dependent typically in the range of 10 
// milliseconds.
//
// TODO: Use C++11 chrono
//

double time() ;


} // namespace util

} // namespace nanonet


#endif // CPP_LIB_UTIL_SYS_H
