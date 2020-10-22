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

#include "ParallelSineBank.hpp"

#include "Base/Assert.hpp"
#include "Constants.hpp"

#include <algorithm>

void ParallelSineBank::setNumThreads(const int numThreads)
{
  assertRelease(numThreads >= 0, "Invalid number of threads");

  mBuffers.resize(numThreads, StereoAudioBuffer{std::vector<float>(kMaxNumFrames, 0.0f),
                                                std::vector<float>(kMaxNumFrames, 0.0f)});
}

const std::vector<Partial>& ParallelSineBank::partials() const { return mPartials; }
void ParallelSineBank::setPartials(std::vector<Partial> partials)
{
  mPartials = std::move(partials);
}

void ParallelSineBank::prepare(const int numActivePartials, const int numFrames)
{
  assertRelease(numActivePartials >= 0, "Invalid number of active partials");
  assertRelease(numFrames > 0 && numFrames <= kMaxNumFrames, "Invalid number of frames");

  mNumActivePartials = numActivePartials;
  mNumTakenPartials = 0;

  for (auto& stereoBuffer : mBuffers)
  {
    std::fill_n(stereoBuffer[0].begin(), numFrames, 0.0f);
    std::fill_n(stereoBuffer[1].begin(), numFrames, 0.0f);
  }
}

int ParallelSineBank::process(const int threadIndex, const int numFrames)
{
  assertRelease(
    threadIndex >= 0 && threadIndex < int(mBuffers.size()), "Invalid thread index");
  assertRelease(numFrames > 0 && numFrames <= kMaxNumFrames, "Invalid number of frames");

  auto& stereoBuffer = mBuffers[threadIndex];

  int numActivePartialsProcessed = 0;
  int partialStartIndex = 0;
  while ((partialStartIndex = mNumTakenPartials.fetch_add(kNumPartialsPerProcessingChunk))
         < int(mPartials.size()))
  {
    const int partialEndIndex =
      std::min(partialStartIndex + kNumPartialsPerProcessingChunk, int(mPartials.size()));
    for (int partialIndex = partialStartIndex; partialIndex < partialEndIndex;
         ++partialIndex)
    {
      auto& partial = mPartials[partialIndex];
      if (partialIndex < mNumActivePartials)
      {
        partial.targetAmp = partial.ampWhenActive;
        ++numActivePartialsProcessed;
      }
      else
      {
        partial.targetAmp = 0.0f;
      }
      processPartial(partial, numFrames, stereoBuffer);
    }
  }

  return numActivePartialsProcessed;
}

void ParallelSineBank::mixTo(const StereoAudioBufferPtrs dest, const int numFrames)
{
  assertRelease(numFrames > 0 && numFrames <= kMaxNumFrames, "Invalid number of frames");

  const auto sumInto = [](const auto& inBuffer, auto* pOutBuffer, const int numFrames) {
    std::transform(inBuffer.begin(), inBuffer.begin() + numFrames, pOutBuffer, pOutBuffer,
                   [](const float x, const float y) { return x + y; });
  };

  for (const auto& buffer : mBuffers)
  {
    sumInto(buffer[0], dest[0], numFrames);
    sumInto(buffer[1], dest[1], numFrames);
  }
}
