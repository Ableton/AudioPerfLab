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

#include "AudioBuffer.hpp"
#include "RampedValue.hpp"

#include <cstdint>

template <typename T>
class VolumeFader
{
public:
  void fadeTo(const T& amp, const uint64_t numFrames)
  {
    mRampedValue.rampTo(amp, numFrames);
  }

  void process(const StereoAudioBufferPtrs ioBuffer, const uint64_t numFrames)
  {
    if (mRampedValue.isRamping() || mRampedValue.value() != T(1))
    {
      for (uint64_t i = 0; i < numFrames; ++i)
      {
        const auto amp = mRampedValue.tick();
        ioBuffer[0][i] *= amp;
        ioBuffer[1][i] *= amp;
      }
    }
  }

private:
  RampedValue<T> mRampedValue{T(1)};
};
