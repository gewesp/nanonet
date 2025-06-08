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
// TODO:
// * Use spatial_index to enable query by name and/or ICAO
// * Blacklisting should be done independently of reading the file.
// * Handle cases of inactive/closed RWYs (e.g. <RWY OPERATIONS="CLOSED">)
//


#include "cpp-lib/geodb.h"

#include "cpp-lib/memory.h"
#include "cpp-lib/units.h"
#include "cpp-lib/util.h"
#include "cpp-lib/sys/syslogger.h"

#include "boost/algorithm/string.hpp"
#include "boost/algorithm/string/predicate.hpp"
#include "boost/lexical_cast.hpp"
#include "boost/property_tree/ptree.hpp"
#include "boost/property_tree/xml_parser.hpp"

// #include <iostream>
#include <exception>
#include <stdexcept>

using namespace cpl::util::log;

namespace {

unsigned type_from_openaip(std::string const& t) {
  if        ("GLIDING" == t 
      || t.find("AF") != std::string::npos 
      || t.find("AD") != std::string::npos) {
    return cpl::gnss::AIRPORT_TYPE_SMALL;
  } else if (t.find("APT") != std::string::npos) {
    return cpl::gnss::AIRPORT_TYPE_LARGE;
  } else if (t.find("HELI") != std::string::npos) {
    return cpl::gnss::AIRPORT_TYPE_HELI;
  } else {
    return 0;
  }
}

namespace {

// Handle runway directions N, NE, ...
bool add_named_direction(
  std::vector<cpl::gnss::runway_data>& rwys,
  const std::string& name) {
  cpl::gnss::runway_data rd;

         if ("N"  == name) {
    rd.magnetic_course =   0;
  } else if ("NE" == name) {
    rd.magnetic_course =  45;
  } else if ("E"  == name) {
    rd.magnetic_course =  90;
  } else if ("SE" == name) {
    rd.magnetic_course = 135;
  } else if ("S"  == name) {
    rd.magnetic_course = 180;
  } else if ("SW" == name) {
    rd.magnetic_course = 225;
  } else if ("W"  == name) {
    rd.magnetic_course = 270;
  } else if ("NW" == name) {
    rd.magnetic_course = 315;
  } else {
    return false;
  }
  
  rwys.push_back(rd);
  return true;
}

} // anonymous namespace


//
// Example XML:
// <RWY OPERATIONS="ACTIVE">
//   <NAME>09/27</NAME>
//   <SFC>GRAS</SFC>
//   <LENGTH UNIT="M">829.6656</LENGTH>
//   <WIDTH UNIT="M">59.7408</WIDTH>
//   <DIRECTION TC="090">
//   </DIRECTION>
//   <DIRECTION TC="270">
//   </DIRECTION>
// </RWY>
//
// Note: The TC appears incorrect in openAIP, and is apparently the magnetic
// course.
//
// Note: As of 12/2020, we don't have reference points for the individual
// runways.  We ignore L and R.
//
// As of 12/2020, we extract the magnetic course from the name(s).
// Typically, one XML entry will result in two runway structs.
//
// Runway names can be 09/27 or E/W, NE/SW, etc.
//

void add_runways_from_openaip(
    std::vector<cpl::gnss::runway_data>& rwys,
    const boost::property_tree::ptree& pt) {
  // Split on '/', e.g. 09R/27L
  std::vector<std::string> names;
  cpl::util::split(names, pt.get<std::string>("NAME"), "/");

  // Remove any L/R
  for (auto& n : names) {
    if (boost::ends_with(n, "L") or boost::ends_with(n, "R")) {
      n.pop_back();
    }
  }

  for (const auto& n : names) {
    // Treat cases of N, NW, E, ..., prevalent in North America
    if (::add_named_direction(rwys, n)) {
      continue;
    } else {
      cpl::gnss::runway_data rd;
      // stoi will silently stop if a non-numeric character
      // is found.  E.g. Grenchen has 25GLD/07GLD
      try {
        rd.magnetic_course = 10 * std::stoi(n);
      } catch (const std::exception& e) {
        throw std::runtime_error("Couldn't parse runway heading: " + n);
      }
      cpl::util::verify_bounds(
          rd.magnetic_course, "magnetic_course", 10, 360);
      rwys.push_back(rd);
    }
  }
}

} // end anonymous namespace

long cpl::gnss::memory_consumption(const cpl::gnss::airport_data& ad) {
  return   cpl::util::memory_consumption(ad.name)
         + cpl::util::memory_consumption(ad.icao)
         + cpl::util::memory_consumption(ad.type)
         + cpl::util::memory_consumption(ad.runways)
         ;
}

std::string cpl::gnss::runway_data::name() const {
  int d = magnetic_course;
  if (d <= 5) { d += 360; }
  char c[30];
  std::sprintf(c, "%02d", (d + 5) / 10);
  return c;
}

cpl::gnss::runway_data cpl::gnss::match_runway(
    const cpl::gnss::airport_data& d, const double course) {
  // No runways: Create one from course
  if (d.runways.empty()) {
    cpl::gnss::runway_data rd;
    rd.magnetic_course = std::round(course);
    return rd;
  }

  double best_delta = 9999.0;
  cpl::gnss::runway_data ret;
  ret.magnetic_course = 9999;

  for (const auto& rwy : d.runways) {
    const double delta = std::abs(
        cpl::math::symmetric_modulo(course - rwy.magnetic_course, 360.0));
    if (delta < best_delta) {
      best_delta = delta;
      ret = rwy;
    }
  }

  return ret;
}

void cpl::gnss::airport_db_from_openaip(
    cpl::gnss::airport_db& ret,
    std::string const& filename,
    bool const capitalize,
    std::ostream* const sl,
    std::set<std::string> const& blacklist) {
  if (sl) {
    *sl << prio::NOTICE << "Airport data: Reading from " 
        << filename
        << std::endl;
  }

  boost::property_tree::ptree pt;
  boost::property_tree::read_xml(filename, pt);

  auto const& wp = pt.get_child("OPENAIP.WAYPOINTS");
  for (auto const& elt : wp) {
    cpl::gnss::lat_lon_alt lla;
    airport_db::value_type v;

    try {
    if ("AIRPORT" != elt.first) {
      continue;
    }
    // Location

    auto const& geoloc = elt.second.get_child("GEOLOCATION");
    lla.lat = geoloc.get<double>("LAT");
    lla.lon = geoloc.get<double>("LON");

    double const alt_some_unit = geoloc.get<double>("ELEV");
    auto const& elev_unit = geoloc.get<std::string>("ELEV.<xmlattr>.UNIT");
    if ("M" == elev_unit) {
      lla.alt = alt_some_unit;
    } else if ("FT" == elev_unit) {
      lla.alt = alt_some_unit * cpl::units::foot();
    } else {
      throw std::runtime_error("openaip reader: unknown unit: " + elev_unit);
    }

    // Data
    auto const& name = elt.second.get_optional<std::string>("NAME");
    auto const& icao = elt.second.get_optional<std::string>("ICAO");

    // Airport name / ICAO
    if (name == boost::none && icao == boost::none) {
      if (sl) {
        *sl << prio::WARNING << "Ignoring airport without NAME nor ICAO"
            << std::endl;
      }
      continue;
    }

    if (name != boost::none) { 
      const int convert = capitalize ? 1 : 0;
      v.name = cpl::util::utf8_canonical(
          name.get(), cpl::util::allowed_characters_1(), convert);

      // Sanity check: Were any characters removed?
      if (capitalize) {
        std::string const compare = cpl::util::utf8_toupper(name.get());
        if (compare != v.name) {
          *sl << prio::WARNING << "Airport name contains invalid characters: "
              << name.get()
              << std::endl;
        }
      }

      if (blacklist.count(v.name)) {
        if (sl) {
          *sl << prio::NOTICE << "Blacklisting airport name " << v.name
              << std::endl;
        }
        continue;
      }
    }
    if (icao != boost::none) { 
      cpl::util::verify_alnum(icao.get());
      v.icao = icao.get();
      cpl::util::toupper(v.icao);
      if (blacklist.count(v.icao)) {
        if (sl) {
          *sl << prio::NOTICE << "Blacklisting airport ICAO code " << v.icao
              << std::endl;
        }
        continue;
      }
    }

    // Airport type (normal, heliport, etc.)
    auto const& t = elt.second.get<std::string>("<xmlattr>.TYPE");
    v.type = type_from_openaip(t);
    if (0 == v.type) {
      if (sl) {
        *sl << prio::WARNING
            << "Couldn't determine airport type; ignoring: "
            << v.name
            << "; type from XML: " << t
            << std::endl;
      }
      continue;
    }

    // Iterate over subtrees in AIRPORT that happen to be a RWY.
    // Since there maybe multiple RWY entries, this doesn't work
    // with get_child().
    // sub will iterate over common subelements of an AIRPORT like
    // <xmlattr>, NAME, ICAO, RADIO, RWY etc.
    for (const auto& sub : elt.second) {
      if ("RWY" == sub.first) {
        ::add_runways_from_openaip(v.runways, sub.second);
      }
    }

    bool log_courses = false;
    // Interesting cases, just FYI
    if (0 != v.runways.size() % 2) {
      log_courses = true;
      if (sl) {
        *sl << prio::NOTICE << "Airport: " << v.name
            << ": Odd number of runways: "
            << v.runways.size()
            << std::endl;
      }
    }

    // Interesting cases, just FYI
    if (10 <= v.runways.size()) {
      log_courses = true;
      if (sl) {
        *sl << prio::NOTICE << "Airport: " << v.name
            << ": Number of runways: " 
            << v.runways.size()
            << std::endl;
      }
    }

    if (sl and log_courses) {
      *sl << prio::NOTICE << "Airport: " << v.name
          << ": Runway magnetic courses: ";
      for (const auto& rwy : v.runways) {
        *sl << rwy.magnetic_course << ' ';
      }
      *sl << std::endl;
    }

    } catch (std::exception const& e) {
      if (sl) {
        *sl << prio::WARNING 
            << "Airport " << v.name << '/' << v.icao
            << ": Skipping due to error: " << e.what()
            << std::endl;
      }
      continue;
    }
    // std::cout << v.type << ','
    //           << v.name << ',' << v.icao << ',' << lla << std::endl;
    ret.add_element(lla, v);
  }
  if (sl) {
    *sl << prio::NOTICE << "Airport data: Read " << ret.size() << " entries"
        << std::endl;
  }
}

void cpl::gnss::airport_db_from_registry(
    cpl::gnss::airport_db& adb,
    cpl::util::registry const& reg,
    std::ostream* const sl) {

  auto const& dir       = reg.check_string("airport_db_directory");
  auto const& countries = reg.check_vector_string("airport_db_country_list");

  cpl::gnss::airport_db_from_openaip(adb, dir, countries, sl);
}

void cpl::gnss::airport_db_from_openaip(
    cpl::gnss::airport_db& adb,
    std::string const& dir,
    std::vector<std::string> const& countries,
    bool const capitalize,
    std::ostream* const sl,
    std::set<std::string> const& blacklist) {

  for (auto const& c : countries) {
    auto const filename = dir + "/" + c + "_wpt.aip";
    airport_db_from_openaip(adb, filename, capitalize, sl, blacklist);
  }
}

cpl::gnss::airport_db
cpl::gnss::airport_db_from_csv(
    std::string const& filename,
    std::ostream* const sl) {
  if (sl) {
    *sl << prio::NOTICE << "Airport data: Reading from " 
        << filename
        << std::endl;
  }

  auto is = cpl::util::file::open_read(filename);
  cpl::gnss::airport_db ret("Airport database " + filename);

  std::string line;
  while (cpl::util::getline(is, line, 1000)) {
    std::vector<cpl::util::stringpiece> items;
    boost::algorithm::split(items, line, boost::algorithm::is_any_of(","));
    if (items.size() != 5) {
      throw std::runtime_error(
          "airport data from csv: " 
          + boost::lexical_cast<std::string>(line)
          + ": expected 5 fields");

    }
    cpl::gnss::lat_lon_alt lla;
    airport_db::value_type v;
    v.icao.assign(items[0].begin(), items[0].end());

    std::string const t(items[1].begin(), items[1].end());
    if        ("small_airport" == t) {
      v.type = AIRPORT_TYPE_SMALL;
    } else if ("medium_airport" == t || "large_airport" == t) {
      v.type = AIRPORT_TYPE_LARGE;
    } else if ("heliport" == t) {
      v.type = AIRPORT_TYPE_HELI;
    } else {
      if (sl) {
        *sl << prio::WARNING << "Ignoring airport: " << v.icao
            << "; type = " << t
            << std::endl;
      }
      continue;
    }

    if (v.icao.size() > 18) {
      throw std::runtime_error(
          "airport data from csv: " 
          + boost::lexical_cast<std::string>(line)
          + ": icao code/name should have <= 18 characters");
    }

    try {
      // lla.lat = cpl::util::stringpice_cast<double>(items[2]);
      lla.lat = boost::lexical_cast<double>(items[2]);
      lla.lon = boost::lexical_cast<double>(items[3]);
      // Sometimes, the altitude value is missing...
      if (!items[4].empty()) {
        lla.alt = boost::lexical_cast<double>(items[4]) * cpl::units::foot();
      }
    } catch (boost::bad_lexical_cast const& e) {
      throw std::runtime_error(
          "airport data from csv: " 
          + boost::lexical_cast<std::string>(line)
          + ": syntax error: " + e.what());
    }
    ret.add_element(lla, v);
  }

  if (sl) {
    *sl << prio::NOTICE << "Airport data: Read " << ret.size() << " entries"
        << std::endl;
  }
  return ret;
}
