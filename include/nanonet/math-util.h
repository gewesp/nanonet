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
// Component: MATH
//


#ifndef NANONET_MATH_UTIL_H
#define NANONET_MATH_UTIL_H

#include "nanonet/assert.h"

#include <algorithm>
#include <functional>
#include <limits>
#include <stdexcept>

#include <cmath>


namespace nanonet {

namespace math {

//
// Discrete model block:  Exponential moving average.
//
// State: Current exponential moving average, NaN if no update has been called.
// Output: none (equivalent with state)
// Parameter: The mix-in factor C.  The state x is updated as
//   x <- (1-C)*x + C*u
//
// C should be close to 0.  The closer to 0 it is, the 'slower' the
// average converges.
//

template< typename T = double >
struct exponential_moving_average {

  typedef T discrete_state_type;
  T default_discrete_state() const
  { return std::numeric_limits<T>::quiet_NaN(); }

  exponential_moving_average(double const& C) : C(C) {
    always_assert(0 < C    );
    always_assert(    C < 1);
  }

  void update_discrete_states(discrete_state_type& x, T const& u) const {
    if (isnan(x)) {
      x = u;
    } else {
      x = (1 - C) * x + C * u;
    }
  }
private:
  double C;
};


///
/// A block to estimate event rates
///
/// Works by maintaining an exponential moving average of the
/// time deltas between events.  
///
struct rate_estimator {
  /// Constructs a rate estimator.
  ///
  /// @param C The mix-in factor, between 0 and 1.  
  ///          The inverse of the rate, dt_est, is updated as
  ///
  ///   dt_est <- (1-C)*dt_est + C*dt
  ///
  /// That is, a higher C means faster updates.
  ///
  /// @param initial_estimate  Initial rate estimate [s^-1]; must be > 0
  rate_estimator(double C, double initial_estimate = 1.0);

  /// Update the estimator with current time which must be monotonically
  /// increasing.  Otherwise, the upate() call is ignored.
  /// @return True iff update was successful (i.e., time was increasing),
  /// false otherwise.
  bool update(double now);

  /// @return Current estimate of event rates [s^-1]
  double estimate() const;

  /// @return Current estimate, also taking into account the estimate()
  /// call itself as an event, but without updating the state.
  double estimate(double now) const;

private:
  exponential_moving_average<double> avg_;
  // See constructor default parameter
  exponential_moving_average<double>::discrete_state_type dt_estimate_ = 1.0;

  // Last update() call
  double last_update_ = std::numeric_limits<double>::quiet_NaN();
};


/// Round to given signed integer type; checking for max value.
template <typename INTEGER>
INTEGER round_to_integer(const double x) {
  const double d = std::round(x);
  if (   d < std::numeric_limits<INTEGER>::min()
      or d > std::numeric_limits<INTEGER>::max()) {
    throw std::runtime_error("value too large to round to integer: " + std::to_string(x));
  }

  const long long i = std::llround(x);
  if (   i < std::numeric_limits<INTEGER>::min()
      or i > std::numeric_limits<INTEGER>::max()) {
    throw std::runtime_error("value too large to round to integer: " + std::to_string(x));
  }

  return static_cast<INTEGER>(i);
}

} // namespace math

} // namespace nanonet


#endif // NANONET_MATH_UTIL_H
