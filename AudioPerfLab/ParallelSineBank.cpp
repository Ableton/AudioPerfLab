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

void ParallelSineBank::prepare(const int numActivePartials)
{
  mNumActivePartials = numActivePartials;
  mNumTakenPartials = 0;
}

void ParallelSineBank::process(const int threadIndex, const int numFrames)
{
  auto& stereoBuffer = mBuffers[threadIndex];

  std::fill_n(stereoBuffer[0].begin(), numFrames, 0.0f);
  std::fill_n(stereoBuffer[1].begin(), numFrames, 0.0f);

  int partialIndex = 0;
  while ((partialIndex = mNumTakenPartials++) < int(mPartials.size()))
  {
    auto& partial = mPartials[partialIndex];
    partial.targetAmp = partialIndex < mNumActivePartials ? partial.ampWhenActive : 0.0f;
    processPartial(partial, numFrames, stereoBuffer);
  }
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
