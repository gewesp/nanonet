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
// Component: SERIALIZATION
//

#ifndef CPP_LIB_SERIALIZATION_SPATIAL_INDEX_H
#define CPP_LIB_SERIALIZATION_SPATIAL_INDEX_H

#include "cpp-lib/spatial-index.h"

#include "cpp-lib/serialization/version.h"

#include <boost/serialization/string.hpp>
#include <boost/serialization/utility.hpp>

#include <boost/serialization/split_free.hpp>

namespace boost {
namespace serialization {

template<typename AR, typename ID, typename T, typename TR, typename STRAT>
void save(
    AR& ar, 
    const cpl::math::spatial_index<ID, T, TR, STRAT>& spi, 
    const unsigned version) {
  ar << spi.name()
     << spi.size()
  ;

  for (const auto& el : spi) {
    ar << el;
  }
}

template<typename AR, typename ID, typename T, typename TR, typename STRAT>
void load(
    AR& ar, 
    cpl::math::spatial_index<ID, T, TR, STRAT>& spi, 
    const unsigned version) {
  std::string name;
  long size = 0;

  ar >> name
     >> size
  ;

  // Initialize to an empty tree
  spi = cpl::math::spatial_index<ID, T, TR, STRAT>(name);

  for (long i = 0; i < size; ++i) {
    typename cpl::math::spatial_index<ID, T, TR, STRAT>::element_type el;
    ar >> el;
    spi.insert_new(el);
  }
}

/// This forwards to one of load()/save() above
/// See https://www.boost.org/doc/libs/1_35_0/libs/serialization/doc/serialization.html#splitting
template<typename AR, typename ID, typename T, typename TR, typename STRAT>
void serialize(
    AR& ar,
    cpl::math::spatial_index<ID, T, TR, STRAT>& spi, 
    const unsigned version) {
  split_free(ar, spi, version);
}

} // end namespace serialization

} // end namespace boost


#endif // CPP_LIB_SERIALIZATION_SPATIAL_INDEX_H
