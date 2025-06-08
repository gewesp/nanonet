//
// Copyright 2014 KISS Technologies GmbH, Switzerland
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

#ifndef CPP_LIB_SERIALIZATION_VERSION_H
#define CPP_LIB_SERIALIZATION_VERSION_H

#include <boost/serialization/version.hpp>

// Class version declarations.  Need to be *outside* of any namespace!!
// These are macros only, so the class doesn't need to be defined yet.
// This file *must* be included by all serialized classes.

// Version history: cpl::gnss::satinfo
// 0               ... Initial version
// 11 (2024-07-27) ... Added vertical_accuracy
BOOST_CLASS_VERSION(cpl::gnss::satinfo, 11)

#endif // CPP_LIB_SERIALIZATION_VERSION_H
