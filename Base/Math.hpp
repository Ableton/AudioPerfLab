/*
 * Copyright (c) 2019 Ableton AG, Berlin
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include <cassert>
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
