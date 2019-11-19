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

#include "Partial.hpp"

#include "Base/Math.hpp"

#include <algorithm>
#include <cmath>
#include <random>

constexpr auto kTwoPi = float(M_PI * 2.0);

std::vector<Partial> generateSaw(const float sampleRate,
                                 const float amp,
                                 const std::chrono::duration<float> ampSmoothingDuration,
                                 const float pan,
                                 const float frequency)
{
  std::vector<Partial> result;

  const auto ampSmoothingCoeff = makeOnePole(ampSmoothingDuration.count(), sampleRate);
  const auto nyquistFrequency = sampleRate / 2.0f;
  const auto numHarmonics = int(nyquistFrequency / frequency);
  for (int i = 1; i <= numHarmonics; ++i)
  {
    Partial partial;
    partial.ampWhenActive =
      (2.0f * amp / float(M_PI)) * (1.0f / i) * (i % 2 == 0 ? 1.0f : -1.0f);
    partial.ampSmoothingCoeff = ampSmoothingCoeff;
    partial.pan = pan;
    const auto partialFrequency = i * frequency;
    const auto samplesPerCycle = sampleRate / partialFrequency;
    partial.phaseIncrement = kTwoPi / samplesPerCycle;

    result.emplace_back(partial);
  }

  return result;
}

std::vector<Partial> generateChord(
  const float sampleRate,
  const std::chrono::duration<float> ampSmoothingDuration,
  const std::vector<float>& noteNumbers)
{
  std::vector<Partial> result;

  for (const auto noteNumber : noteNumbers)
  {
    const auto frequency = noteToFrequency(noteNumber);

    const auto appendPartials = [&](const auto amp, const auto pan, const auto detune) {
      const auto partials =
        generateSaw(sampleRate, amp, ampSmoothingDuration, pan, frequency + detune);
      std::copy(partials.begin(), partials.end(), std::back_insert_iterator(result));
    };

    const auto amp = 1.0f / (noteNumbers.size() * 5);
    appendPartials(amp, -1.0f, -4.0f);
    appendPartials(amp, -1.0f, -2.0f);
    appendPartials(amp, 0.0f, 0.0f);
    appendPartials(amp, 1.0f, 2.0f);
    appendPartials(amp, 1.0f, 4.0f);
  }

  std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
    return a.phaseIncrement < b.phaseIncrement;
  });

  return result;
}

std::vector<Partial> randomizePhases(std::vector<Partial> partials,
                                     const int partialsToSkip)
{
  std::default_random_engine generator{42};
  std::normal_distribution<float> phaseDistribution(0.0, kTwoPi);
  const auto iFirst = std::min(partials.begin() + partialsToSkip, partials.end());
  std::transform(iFirst, partials.end(), iFirst, [&](Partial partial) {
    partial.phase = phaseDistribution(generator);
    return partial;
  });
  return partials;
}

void processPartial(Partial& partial, const int numFrames, StereoAudioBuffer& output)
{
  const auto kSilenceThreshold = 0.00001f;
  if (std::fabs(partial.targetAmp) > kSilenceThreshold
      || std::fabs(partial.amp) > kSilenceThreshold)
  {
    const auto channelAmps = equalPowerPanGains(partial.pan);
    for (int frameIndex = 0; frameIndex < numFrames; ++frameIndex)
    {
      const auto sample = std::sin(partial.phase) * partial.amp;
      output[0][frameIndex] += sample * channelAmps.first;
      output[1][frameIndex] += sample * channelAmps.second;

      partial.amp = lerp(partial.amp, partial.targetAmp, partial.ampSmoothingCoeff);

      partial.phase += partial.phaseIncrement;
      if (partial.phase >= kTwoPi)
      {
        partial.phase -= kTwoPi;
      }
    }
  }
}
