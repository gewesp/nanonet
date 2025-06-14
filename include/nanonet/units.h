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
// TODO:
// - C++11: Use constexpr once supported on all compilers.
//


#ifndef CPP_LIB_UNITS_H
#define CPP_LIB_UNITS_H

namespace nanonet {

namespace units {

// Time [second].
inline double constexpr day   () { return 86400. ; } 
inline double constexpr hour  () { return 3600.  ; }
inline double constexpr minute() { return 60.    ; } 

inline double constexpr year  () { return 365 * day() ; }

inline double constexpr millisecond() { return 1e-3; }


} // namespace units

} // namespace nanonet


#endif // CPP_LIB_UNITS_H
