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

//
// DO NOT DIRECTLY INCLUDE FROM APPLICATION CODE.
//

#include "nanonet/windows/wrappers.h"


namespace nanonet {

namespace util {

//
// Schedule periodic events with fixed time delta.  Bound to some
// Windows realtime clock.
//

struct realtime_scheduler {

  // Construct with given time delta dt [s].
  realtime_scheduler( double const& dt ) ;

  // Wait (sleep) until next slice and return current time [s].  
  double wait_next() ;

  // Get current time [s].
  double time() { return nanonet::detail_::windows_time() ; }

private:

  nanonet::detail_::auto_handle timer ;

} ;


} // namespace util

} // namespace nanonet
