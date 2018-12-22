// Copyright: 2018, Ableton AG, Berlin. All rights reserved.

#pragma once

#include "AudioBuffer.hpp"

#include <array>
#include <chrono>
#include <vector>

struct Partial
{
  float ampWhenActive{};
  float targetAmp{};
  float amp{};
  float ampSmoothingCoeff{};

  float pan{};

  float phaseIncrement{};
  float phase{};
};

std::vector<Partial> generateSaw(float sampleRate,
                                 float amp,
                                 std::chrono::duration<float> ampSmoothingDuration,
                                 float pan,
                                 float frequency);

std::vector<Partial> generateChord(float sampleRate,
                                   std::chrono::duration<float> ampSmoothingDuration,
                                   const std::vector<float>& noteNumbers);

void processPartial(Partial& partial, int numFrames, StereoAudioBuffer& output);
