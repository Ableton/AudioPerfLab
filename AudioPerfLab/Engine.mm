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

#import "Engine.hpp"

#include "Constants.hpp"
#include "ParallelSineBank.hpp"
#include "Partial.hpp"

#include "Base/Assert.hpp"
#include "Base/AudioHost.hpp"
#include "Base/BusyThreads.hpp"
#include "Base/Driver.hpp"
#include "Base/FixedSPSCQueue.hpp"
#include "Base/Math.hpp"
#include "Base/Semaphore.hpp"
#include "Base/Thread.hpp"

#include <CoreAudio/CoreAudioTypes.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <optional>
#include <thread>
#include <vector>

namespace
{

float peakLevel(const StereoAudioBufferPtrs input, const int numFrames)
{
  float result = 0.0;
  for (int i = 0; i < numFrames; ++i)
  {
    result = std::max({result, std::abs(input[0][i]), std::abs(input[1][i])});
  }
  return result;
}

PerformanceConfig presetToConfig(const PerformancePreset preset)
{
  switch (preset)
  {
  case standardPreset:
    return kStandardPerformanceConfig;

  case optimalPreset:
    return kOptimalPerformanceConfig;

  case customPreset:
    fatalError("No config for the custom preset");
  }
}

PerformancePreset configToPreset(const PerformanceConfig& config)
{
  if (config == kStandardPerformanceConfig)
  {
    return standardPreset;
  }
  else if (config == kOptimalPerformanceConfig)
  {
    return optimalPreset;
  }
  else
  {
    return customPreset;
  }
}

} // namespace

class EngineImpl
{
  using Clock = std::chrono::high_resolution_clock;

public:
  EngineImpl()
    : mHost{[&](const int numProcessingThreads) { setup(numProcessingThreads); },
            [&](const StereoAudioBufferPtrs ioBuffer, const int numFrames) {
              renderStarted(ioBuffer, numFrames);
            },
            [&](const int threadIndex, const int numFrames) {
              process(threadIndex, numFrames);
            },
            [&](const StereoAudioBufferPtrs ioBuffer,
                const uint64_t hostTime,
                const int numFrames) { renderEnded(ioBuffer, hostTime, numFrames); }}
  {
    const auto chordPartials = generateChord(
      mHost.driver().sampleRate(), kAmpSmoothingDuration, kChordNoteNumbers);
    mSineBank.setPartials(randomizePhases(chordPartials, kNumUnrandomizedPhases));
    mHost.start();
  }

  AudioHost& host() { return mHost; }
  BusyThreads& busyThreads() { return mBusyThreads; }

  PerformanceConfig performanceConfig() const
  {
    return {.busyThreads = mBusyThreads.config(), .audioHost = mHost.config()};
  }
  void setPerformanceConfig(const PerformanceConfig& config)
  {
    mBusyThreads.setConfig(config.busyThreads);
    mHost.setConfig(config.audioHost);
  }

  int numSines() const { return mNumSines; }
  void setNumSines(const int numSines) { mNumSines = numSines; }

  int maxNumSines() const { return int(mSineBank.partials().size()); }

  void playSineBurst(const double duration, const int numAdditionalSines)
  {
    mNumAdditionalSinesInBurst = numAdditionalSines;
    mSineBurstDuration = duration;
  }

  std::optional<DriveMeasurement> popDriveMeasurement()
  {
    const auto* pMeasurement = mDriveMeasurements.front();
    const auto result = pMeasurement ? std::make_optional(*pMeasurement) : std::nullopt;
    mDriveMeasurements.popFront();
    return result;
  }

private:
  void addDriveMeasurement(const uint64_t hostTime,
                           const std::chrono::time_point<Clock> bufferStartTime,
                           const std::chrono::time_point<Clock> bufferEndTime,
                           const int numFrames,
                           const float inputPeakLevel)
  {
    DriveMeasurement driveMeasurement{};
    driveMeasurement.hostTime = machAbsoluteTimeToSeconds(hostTime).count();
    driveMeasurement.duration =
      std::chrono::duration<double>{bufferEndTime - bufferStartTime}.count();
    driveMeasurement.numFrames = numFrames;
    std::copy(mNumActivePartialsProcessed.begin(), mNumActivePartialsProcessed.end(),
              driveMeasurement.numActivePartialsProcessed);
    std::copy(mCpuNumbers.begin(), mCpuNumbers.end(), driveMeasurement.cpuNumbers);
    driveMeasurement.inputPeakLevel = inputPeakLevel;
    mDriveMeasurements.tryPushBack(driveMeasurement);
  }

  // Called with no audio threads active after app launch and setting changes
  void setup(const int numProcessingThreads)
  {
    assertRelease(
      (numProcessingThreads + 1) <= MAX_NUM_THREADS, "Too many processing threads");

    mSineBank.setNumThreads(numProcessingThreads);
    std::fill(mNumActivePartialsProcessed.begin(), mNumActivePartialsProcessed.end(), -1);
    std::fill(mCpuNumbers.begin(), mCpuNumbers.end(), -1);
  }

  // Called at the start of the audio I/O callback with no worker threads active
  void renderStarted(StereoAudioBufferPtrs, const int numFrames)
  {
    mRenderStartTime = Clock::now();

    if (const auto duration = mSineBurstDuration.exchange(0.0f))
    {
      mNumSineBurstSamplesRemaining = float(mHost.driver().sampleRate()) * duration;
    }

    const auto effectiveNumSines =
      mNumSines.load()
      + (mNumSineBurstSamplesRemaining > 0 ? mNumAdditionalSinesInBurst.load() : 0);
    mSineBank.prepare(effectiveNumSines, numFrames);

    if (!mHost.processInDriverThread())
    {
      mNumActivePartialsProcessed[0] = -1;
      mCpuNumbers[0] = cpuNumber();
    }
  }

  // Called by the main audio I/O thread (if processing in the driver thread is enabled)
  // and by worker threads
  void process(const int threadIndex, const int numFrames)
  {
    const auto processingThreadIndex =
      threadIndex - (host().processInDriverThread() ? 0 : 1);
    mNumActivePartialsProcessed[threadIndex] =
      mSineBank.process(processingThreadIndex, numFrames);
    mCpuNumbers[threadIndex] = cpuNumber();
  }

  // Called at the end of the audio I/O callback with no worker threads active
  void renderEnded(const StereoAudioBufferPtrs ioBuffer,
                   const uint64_t hostTime,
                   const int numFrames)
  {
    const auto inputPeakLevel = peakLevel(ioBuffer, numFrames);
    std::fill_n(ioBuffer[0], numFrames, 0.0f);
    std::fill_n(ioBuffer[1], numFrames, 0.0f);

    mSineBank.mixTo(ioBuffer, numFrames);

    mNumSineBurstSamplesRemaining =
      std::max<int>(0, mNumSineBurstSamplesRemaining - numFrames);

    const auto endTime = Clock::now();
    addDriveMeasurement(hostTime, mRenderStartTime, endTime, numFrames, inputPeakLevel);
  }

  AudioHost mHost;
  BusyThreads mBusyThreads;
  ParallelSineBank mSineBank;
  Clock::time_point mRenderStartTime;
  FixedSPSCQueue<DriveMeasurement> mDriveMeasurements{kDriveMeasurementQueueSize};
  std::atomic<int> mNumSines{kDefaultNumSines};

  std::atomic<int> mNumAdditionalSinesInBurst{0};
  std::atomic<float> mSineBurstDuration{0.0f};
  int mNumSineBurstSamplesRemaining{0};

  std::array<std::atomic<int>, MAX_NUM_THREADS> mNumActivePartialsProcessed{};
  std::array<std::atomic<int>, MAX_NUM_THREADS> mCpuNumbers{};
};

@implementation Engine
{
  EngineImpl mEngine;
}

- (PerformancePreset)preset { return configToPreset(mEngine.performanceConfig()); }
- (void)setPreset:(PerformancePreset)preset
{
  mEngine.setPerformanceConfig(presetToConfig(preset));
}

- (bool)isAudioInputEnabled { return mEngine.host().isAudioInputEnabled(); }
- (void)setIsAudioInputEnabled:(bool)enabled
{
  mEngine.host().setIsAudioInputEnabled(enabled);
}

- (float)outputVolume { return mEngine.host().driver().outputVolume(); }
- (void)setOutputVolume:(float)outputVolume fadeDuration:(double)fadeDuration
{
  mEngine.host().driver().setOutputVolume(outputVolume, Driver::Seconds{fadeDuration});
}

- (int)preferredBufferSize { return mEngine.host().preferredBufferSize(); }
- (void)setPreferredBufferSize:(int)preferredBufferSize
{
  mEngine.host().setPreferredBufferSize(preferredBufferSize);
}

- (double)sampleRate { return mEngine.host().driver().sampleRate(); }

- (int)numWorkerThreads { return mEngine.host().numWorkerThreads(); }

- (int)numProcessingThreads { return mEngine.host().numProcessingThreads(); }
- (void)setNumProcessingThreads:(int)numThreads
{
  mEngine.host().setNumProcessingThreads(numThreads);
}

- (int)numBusyThreads { return mEngine.busyThreads().numThreads(); }
- (void)setNumBusyThreads:(int)numThreads
{
  mEngine.busyThreads().setNumThreads(numThreads);
}

- (double)busyThreadPeriod { return mEngine.busyThreads().period().count(); }
- (void)setBusyThreadPeriod:(double)period
{
  mEngine.busyThreads().setPeriod(BusyThreads::Seconds{period});
}

- (double)busyThreadCpuUsage { return mEngine.busyThreads().threadCpuUsage(); }
- (void)setBusyThreadCpuUsage:(double)percentage
{
  mEngine.busyThreads().setThreadCpuUsage(percentage);
}

- (bool)processInDriverThread { return mEngine.host().processInDriverThread(); }
- (void)setProcessInDriverThread:(bool)enabled
{
  mEngine.host().setProcessInDriverThread(enabled);
}

- (bool)isWorkIntervalOn { return mEngine.host().isWorkIntervalOn(); }
- (void)setIsWorkIntervalOn:(bool)isOn { mEngine.host().setIsWorkIntervalOn(isOn); }

- (double)minimumLoad { return mEngine.host().minimumLoad(); }
- (void)setMinimumLoad:(double)minimumLoad { mEngine.host().setMinimumLoad(minimumLoad); }

- (int)numSines { return mEngine.numSines(); }
- (void)setNumSines:(int)numSines { mEngine.setNumSines(numSines); }

- (int)maxNumSines { return mEngine.maxNumSines(); }

- (void)playSineBurstFor:(double)duration additionalSines:(int)numAdditionalSines
{
  mEngine.playSineBurst(duration, numAdditionalSines);
}

- (void)fetchMeasurements:(void (^)(struct DriveMeasurement))callback
{
  while (const auto maybeMeasurement = mEngine.popDriveMeasurement())
  {
    callback(*maybeMeasurement);
  }
}

@end
