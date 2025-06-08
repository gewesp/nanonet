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

#ifndef CPP_LIB_SERIALIZATION_GNSS_H
#define CPP_LIB_SERIALIZATION_GNSS_H

#include "cpp-lib/gnss.h"

#include "cpp-lib/serialization/version.h"

#include <boost/serialization/base_object.hpp>


namespace boost {
namespace serialization {

template <typename AR>
void serialize(AR& ar, cpl::gnss::satinfo& sat, const unsigned int version) {
  ar & sat.n_satellites
     & sat.horizontal_accuracy
  ;

  if (version >= 11) {
     ar & sat.vertical_accuracy;
  }
}

template <typename AR>
void serialize(AR& ar, cpl::gnss::lat_lon& ll, const unsigned int version) {
  ar & ll.lat
     & ll.lon
  ;
}

template <typename AR>
void serialize(AR& ar, cpl::gnss::lat_lon_alt& lla, const unsigned int version) {
  ar & boost::serialization::base_object<cpl::gnss::lat_lon>(lla);
  ar & lla.alt;
}

template <typename AR>
void serialize(AR& ar, cpl::gnss::position_time& pt, const unsigned int version) {
  ar & boost::serialization::base_object<cpl::gnss::lat_lon_alt>(pt);
  ar & pt.time;
}

template <typename AR>
void serialize(AR& ar, cpl::gnss::fix& f, const unsigned int version) {
  ar & boost::serialization::base_object<cpl::gnss::position_time>(f)
     & boost::serialization::base_object<cpl::gnss::satinfo>      (f)
  ;
}

template <typename AR>
void serialize(AR& ar, cpl::gnss::motion& mo, const unsigned int version) {
  ar & mo.speed
     & mo.course
     & mo.vertical_speed
  ;
}

template <typename AR>
void serialize(AR& ar, cpl::gnss::motion_and_turnrate& mot, const unsigned int version) {
  ar & boost::serialization::base_object<cpl::gnss::motion>(mot)
     & mot.turnrate
  ;
}

} // end namespace serialization
} // end namespace boost

#endif // CPP_LIB_SERIALIZATION_GNSS_H
