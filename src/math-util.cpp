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

#include "nanonet/math-util.h"


nanonet::math::rate_estimator::rate_estimator(
    const double C, const double initial_estimate)
: avg_(C),
  dt_estimate_(1 / initial_estimate) {
  always_assert(initial_estimate > 0);
}

bool nanonet::math::rate_estimator::update(const double now) {
  if (now != now) {
    return false;
  }

  if (last_update_ == last_update_) {
    const double dt = now - last_update_;
    if (dt >= 0) {
      avg_.update_discrete_states(dt_estimate_, dt);
      last_update_ = now;
      return true;
    } else {
      return false;
    }
  } else {
    last_update_ = now;
    return true;
  }
}

double nanonet::math::rate_estimator::estimate() const {
  return 1 / dt_estimate_;
}

double nanonet::math::rate_estimator::estimate(const double now) const {
  nanonet::math::rate_estimator copy = *this;
  copy.update(now);
  return copy.estimate();
}
