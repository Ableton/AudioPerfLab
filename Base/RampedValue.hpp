/*
 * Copyright (c) 2020 Ableton AG, Berlin
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

#include <cstdint>

/*! A linear ramped value
 *
 *  The value will ramp linearly to the target value so that when ramping from [x -> y]
 *  over N samples, the first value of the ramp will be x, and the Nth will be y.
 *
 *  @note The behaviour of RampedValue<float> is precise up to durations of around 2
 *        minutes. For longer durations the ramp may finish slightly early.
 */
template <typename T>
class RampedValue
{
public:
  explicit RampedValue(const T& value = T(0))
    : mCurrent(value)
    , mTarget(value)
    , mIncrement(T(0))
    , mTicksToCompletion(0u)
    , mDurationInTicks(0)
  {
  }

  //! Sets the current value and disables any ramping
  void setValue(const T& value)
  {
    mCurrent = value;
    mTarget = value;
    mTicksToCompletion = 0u;
    mDurationInTicks = 0u;
  }

  //! Start a linear ramp towards the target value over the specified number of ticks
  void rampTo(const T& target, const uint64_t ticksToCompletion)
  {
    mTarget = target;
    mDurationInTicks = ticksToCompletion;

    if (ticksToCompletion <= 1u || mTarget == mCurrent)
    {
      mCurrent = target;
      mTicksToCompletion = 0u;
    }
    else
    {
      mTicksToCompletion = ticksToCompletion - 1u;
      mIncrement = (mTarget - mCurrent) / T(mTicksToCompletion);
    }
  }

  //! Returns true if the value is currently ramping
  bool isRamping() const { return mTicksToCompletion > 0u; }

  //! Returns the current value
  T value() const { return mCurrent; }

  //! Returns the target value
  T targetValue() const { return mTarget; }

  //! Returns the next value in the ramp
  T tick()
  {
    const auto result = mCurrent;

    if (mTicksToCompletion > 0u)
    {
      --mTicksToCompletion;
      mCurrent = mTarget - T(mTicksToCompletion) * mIncrement;
    }
    else
    {
      // We assign the target to the value here to avoid slight under or over-shoots
      mCurrent = mTarget;
    }

    return result;
  }

private:
  T mCurrent;
  T mTarget;
  T mIncrement;
  uint64_t mTicksToCompletion;
  uint64_t mDurationInTicks;
};
