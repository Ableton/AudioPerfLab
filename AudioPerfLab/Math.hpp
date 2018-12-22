// Copyright: 2018, Ableton AG, Berlin. All rights reserved.

#pragma once

#include <cmath>
#include <utility>

/*! Returns equal power gains for a specified pan position.
 *
 *  Introduces a decrease in gain of 3dB for both channels for the central pan position.
 *
 *  @param pan must be a value between -1 (left) and 1 (right) with 0 in the center.
 */
template <typename T>
std::pair<T, T> equalPowerPanGains(const T& pan)
{
  assert(pan >= T(-1) && pan <= T(1));

  return {std::sin(T(M_PI_4) * (T(1) - pan)), std::sin(T(M_PI_4) * (pan + T(1)))};
}

/*! One-Pole Coefficient Generation
 *
 * Calculates the feed-forward one-pole coefficient so that the difference equation
 *
 *   `y[n] = y[n - 1] + (x[n] - y[n - 1]) * makeOnePole(tau, fs)`
 *
 * realizes a low-pass filter with time constant `tau` relative to sample rate `fs`.
 */
template <typename T>
T makeOnePole(const T& tau, const T& fs)
{
  assert(tau >= T(0) && fs > T(0));

  using std::max;
  return T(1) - std::exp(-T(1) / max(tau * fs, T(1.0e-6)));
}

//! Linear interpolation/extrapolation
template <typename T, typename X>
T lerp(const T& a, const T& b, const X& x)
{
  return (T(1) - x) * a + x * b;
}

constexpr int kNoteC3 = 60;
constexpr int kNoteA3 = kNoteC3 + 9;

template <typename T>
T noteToFrequency(const T& note, const T& reference = T(440))
{
  return std::exp2((note - T(kNoteA3)) / T(12)) * reference;
}
