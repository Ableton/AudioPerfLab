// Copyright: 2018, Ableton AG, Berlin. All rights reserved.

#include "ParallelSineBank.hpp"

#include "Constants.hpp"

#include <algorithm>

void ParallelSineBank::setNumThreads(const int numThreads)
{
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

void ParallelSineBank::mixTo(const std::array<float*, 2>& dest, const int numFrames)
{
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
