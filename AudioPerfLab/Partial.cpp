// Copyright: 2018, Ableton AG, Berlin. All rights reserved.

#include "Partial.hpp"

#include "Math.hpp"

#include <algorithm>
#include <cmath>

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
    partial.phaseIncrement = 2.0f * float(M_PI) / samplesPerCycle;

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
    appendPartials(amp, -1.0f, -2.0f);
    appendPartials(amp, 0.5f, -1.0f);
    appendPartials(amp, 0.0f, 0.0f);
    appendPartials(amp, 0.5f, 1.0f);
    appendPartials(amp, 1.0f, 2.0f);
  }

  std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
    return a.phaseIncrement < b.phaseIncrement;
  });

  return result;
}

void processPartial(Partial& partial, const int numFrames, StereoAudioBuffer& output)
{
  constexpr auto kTwoPi = float(M_PI * 2.0);

  const auto kSilenceThreshold = 0.00001f;
  if (partial.targetAmp > kSilenceThreshold || partial.amp > kSilenceThreshold)
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
