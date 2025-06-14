//
// Copyright 2019 and onwards by KISS Technologies GmbH, Switzerland
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

#ifndef NANONET_DETAIL_PATFORM_WRAPPERS_H
#define NANONET_DETAIL_PATFORM_WRAPPERS_H

#include "./platform_definition.h"

#if (BOOST_OS_LINUX || BOOST_OS_MACOS)
#  include "nanonet/posix/wrappers.h"
#elif (BOOST_OS_WINDOWS)
#  include "nanonet/windows/wrappers.h"
#else
#  error "This operating system platform is not supported by nanonet."
#endif

#endif // NANONET_DETAIL_PATFORM_WRAPPERS_H
