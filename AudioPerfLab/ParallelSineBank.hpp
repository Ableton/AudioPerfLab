// Copyright: 2018, Ableton AG, Berlin. All rights reserved.

#pragma once

#include "AudioBuffer.hpp"
#include "Partial.hpp"

#include <array>
#include <atomic>
#include <vector>

class ParallelSineBank
{
public:
  void setNumThreads(int numThreads);

  const std::vector<Partial>& partials() const;
  void setPartials(std::vector<Partial> partials);

  void prepare(int numActivePartials);
  void process(int threadIndex, int numFrames);
  void mixTo(const std::array<float*, 2>& dest, int numFrames);

private:
  std::vector<Partial> mPartials;
  std::vector<StereoAudioBuffer> mBuffers;
  std::atomic<int> mNumActivePartials{0};
  std::atomic<int> mNumTakenPartials{0};
};
