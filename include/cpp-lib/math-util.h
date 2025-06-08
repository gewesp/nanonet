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


#ifndef CPP_LIB_MATH_UTIL_H
#define CPP_LIB_MATH_UTIL_H

#include <algorithm>
#include <exception>
#include <functional>
#include <limits>
#include <stdexcept>
#include <sstream>
#include <vector>
// #include <iostream>

#include <cassert>
#include <cmath>

#include "cpp-lib/assert.h"

//
// These shouldn't be in C++, but some systems do declare them as macros.  They
// only are in C99, which is not part of C++.  We declare our own isinf and
// isnan functions below.
//

#undef isnan
#undef isinf

namespace cpl {


namespace math     {





/// Maximum of three values.

/// \return max( x1 , x2 , x3 ).

// Microsoft Compiler Version 13.10.3077 for 80x86 dies with fatal error
// C1001: INTERNAL COMPILER ERROR on this...
#ifndef _MSC_VER
template< typename T >
T max( T const& x1 , T const& x2 , T const& x3 ) {

  return std::max( x1 , std::max( x2 , x3 ) ) ;

}
#endif


/// Clamp value to a range.

/// \reval x Is set to \a lower if \a x < \a lower or \a upper if \a x >
/// \a upper.  Otherwise, \a x is left alone.
/// \precond \a lower <= \a upper.

template< typename T >
inline void clamp( T& x , T const& lower , T const& upper ) {
  assert( lower <= upper ) ;
  if     ( x < lower ) { x = lower ; }
  else if( x > upper ) { x = upper ; }
}


//
// Return the relative error | ( a - b ) / b | for b != 0.
//

inline double relative_error( double const& a , double const& b ) {
  assert( b != 0 ) ;
  return std::fabs( ( a - b ) / b ) ;
}


//
// Approximate equality.  Returns true iff either x1 = 0 and x2 = 0, or
// x1 and x2 are nonzero, have the same sign and
//
//   | x1 / x2 | <= 1 + f eps and | x2 / x1 | <= 1 + f eps
//
// where eps = numeric_limits<double>::epsilon().
//

bool approximate_equal
( double const& x1 , double const& x2 , double const& f = 1 ) ;


/// Minimum of positive or undefined values.

/// \return min( \a x1 , \a x2 ), where negative values of the input
/// parameters stand for undefined values.  If one of the parameters
/// is negative, the other parameter is returned.

template< typename T >
T pos_min( T const& x1 , T const& x2 ) {

  if( x1 < 0 ) { return x2 ; }
  if( x2 < 0 ) { return x1 ; }

  assert( x1 >= 0 ) ;
  assert( x2 >= 0 ) ;

  return std::min( x1 , x2 ) ;

}

//
// TODO: average and exponential_moving_average don't fit the 'discrete model
// block' concept.  Introduce a separate discrete time modelling concept.
//
// Discrete model block:  Average.
//
// State: Sum and number of added elements.
// Output: result (the average), or a default value if no update has been done.
//

/// Class holding the state for an averager
/// Used to be a nested struct of average, but this causes serialization
/// to fail.
template< typename T = double >
struct average_state_type {
  /// Sum of elements added so far
  T sum = T();

  /// Count of elements added so far
  double n = 0;
};

template< typename T = double >
struct average {
  average() {}

  using discrete_state_type = average_state_type<T>;

  discrete_state_type default_discrete_state() const
  { return discrete_state_type(); }

  void update_discrete_states(discrete_state_type& x, T const& u) const {
    ++x.n;
    x.sum += u;
  }

  T outputs(discrete_state_type const& x, T const& default_value = 0.0) const {
    if (x.n < .5) {
      return default_value;
    } else {
      return x.sum / x.n;
    }
  }
};


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

/// Numerical derivative of a vector.

/// Compute numerical derivative of according to the symmetric formula
/// \f[
/// f^{ \prime }( x ) \approx \frac{ f( x + h ) - f( x - h ) }{ 2h }.
/// \f]
/// At the beginning and end of the vector, the asymmetric formula is
/// used.
///
/// \param v Tabulated function values, 1/\a f apart.
/// \param f Sampling frequency.
///
/// \return Tabulated function values of the derivative.  Same number of
/// elements as in \a v.

template< typename T >
std::vector< T >
derivative( std::vector< T > const& v , double const& f ) ;


/// Convert \a cont to std::vector< double >

/// \return Some sequence of some element type converted to a
/// double vector.

template< typename C >
inline std::vector< double >
to_double_vector( C const& cont ) {

  return std::vector< double >( cont.begin() , cont.end() ) ;

}


/// Report locations and values of Minima and Maxima in a vector.

/// The message format is:
///
///   name: maximum max at t = t_max , minimum min at t = t_min \n
///
/// \param os Stream to write to.
/// \param v The vector.
/// \param name The beginning of the message string.
/// \param frequency Sampling frequency, used for time
/// computation.

template< typename T >
void report_minmax(
  std::ostream& os ,
  std::vector< T > const& v ,
  std::string const& name ,
  long frequency
) ;


/// Sample a function.

/// Fill a container with sampled values, starting at t0 with frequency f.
///
/// \retval cont Where to write to.
/// \param function A unary function.
/// \param t0 The starting time.
/// \param f Frequency.

template< typename C , typename F , typename T >
void sample(
  C& cont ,
  F const& function ,
  T const& t0 ,
  T const& f
) ;


/// Multiply all elements of container \a c by \a x.

template< typename C >
void multiply( C& c , typename C::value_type const& x ) ;


/// assert() that all elements in range are >= 0

/// \param begin Iterator pointing to the beginning element of the range.
/// \param end Iterator pointing to the past-the-end element of the
/// range.

template< typename for_it >
void assert_nonneg( for_it const& begin , for_it const& end ) ;


/// Do downsampling.

/// Return a vector where each element is the mean of n succesive
/// elements of cont.  The cont.size() % n last elements of cont are
/// ignored.
///
/// \param cont The original value sequence.
/// \param n Downsampling factor.
/// \return The vector of downsampled values.

template< typename C >
std::vector< typename C::value_type >
down_sample( C const& cont , long n ) {

  assert( n >= 1 ) ;

  typedef typename C::value_type T ;

  std::vector< T > ret( cont.size() / n ) ;

  for( typename std::vector< T >::size_type i = 0 ; i < ret.size() ; ++i ) {

    T x = 0 ;

    for( long j = 0 ; j < n ; ++j )
      x += cont[ n * i + j ] ;

    ret[ i ] = x / n ;

  }

  return ret ;

}

// Safe division function.  Returns 0.0 in case of division
// by zero.
inline double safe_divide(const double num, const double den) {
  if (0.0 == den) {
    return 0.0;
  } else {
    return num / den;
  }
}

// Safe function to calculate percentages.  Returns 0.0 in case
// of divisions by zero.
inline double percentage(const double& value, const double& reference) {
  return 100.0 * safe_divide(value, reference);
}

// Generalized round to the next multiple of C, e.g. C = 100.  Can be
// used with any C > 0, including fractional, e.g. C = 0.25 to round 
// to quarters.
inline double round(const double x, const double C) {
  assert(C > 0);
  return C * std::round(x / C);
}

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

} // namespace cpl


#endif // CPP_LIB_MATH_UTIL_H
