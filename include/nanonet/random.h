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
// Component: UNSUPPORTED
//
// TODO: Replace by C++11 <random> facilities
//


#ifndef NANONET_RANDOM_H
#define NANONET_RANDOM_H


namespace nanonet {

namespace util {

//
// Returns a random sequence whose size is drawn from sd and values from
// vd.
//

template<typename cont, typename rng, typename sizedist, typename valdist>
cont random_sequence(rng& rand, sizedist& sd, valdist& vd) {
  unsigned long const size = sd(rand);
  cont ret;

  // ret.reserve(size);

  for (unsigned long i = 0; i < size; ++i) {
    ret.push_back(vd(rand));
  }
  return ret;
}
    

} // namespace util

} //  nanonet


#endif // NANONET_RANDOM_H
