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

#ifndef CPP_LIB_SERIALIZATION_OGN_H
#define CPP_LIB_SERIALIZATION_OGN_H

#include "cpp-lib/ogn.h"

#include "cpp-lib/serialization/version.h"

#include <boost/serialization/array.hpp>
#include <boost/serialization/set.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/utility.hpp>


namespace boost {
namespace serialization {

template <typename AR>
void serialize(AR& ar, cpl::ogn::versions& ver, const unsigned int version) {
  ar & ver.hardware;
  ar & ver.software;
}

template <typename AR>
void serialize(AR& ar, cpl::ogn::vehicle_data& d, const unsigned int version) {
  ar & d.name1
     & d.name2
     & d.type
     & d.tracking
     & d.identify
     & d.id_type_probably_wrong
  ;
}

template <typename AR>
void serialize(AR& ar, cpl::ogn::aprs_info& aprs, const unsigned int version) {
  ar & aprs.tocall
     & aprs.relay
     & aprs.from
  ;
}

template <typename AR>
void serialize(AR& ar, cpl::ogn::rx_info& rx, const unsigned int version) {
  ar & rx.received_by
     & rx.rssi
     & rx.frequency_deviation
     & rx.errors
     & rx.is_relayed
     & rx.aprs
  ;
}

template <typename AR>
void serialize(AR& ar, cpl::ogn::aircraft_rx_info& rx, const unsigned int version) {
  ar & rx.id_type;
  ar & rx.vehicle_type;
  ar & rx.process;
  ar & rx.stealth;
  ar & rx.ver;
  ar & rx.data;
  ar & rx.pta;
  ar & rx.mot;
  ar & rx.baro_alt;
  ar & rx.rx;
}

template <typename AR>
void serialize(AR& ar, cpl::ogn::station_info& si, const unsigned int version) {
  ar & si.network
     & si.pt

     & si.cpu_load

     & si.ram_used_mb
     & si.ram_available_mb

     & si.ntp_difference
     & si.ntp_ppm

     & si.temperature

     & si.version
  ;
}

} // end namespace serialization
} // end namespace boost


#endif // CPP_LIB_SERIALIZATION_OGN_H
