// Copyright: 2018, Ableton AG, Berlin. All rights reserved.

#import "Engine.hpp"

#include "Constants.hpp"
#include "ParallelSineBank.hpp"
#include "Partial.hpp"

#include "Base/Assert.hpp"
#include "Base/AudioHost.hpp"
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

class EngineImpl
{
  using Clock = std::chrono::high_resolution_clock;

public:
  EngineImpl()
  {
    mHost.emplace(
      [&](const int numWorkerThreads) { setup(numWorkerThreads); },
      [&](const int numFrames) { renderStarted(numFrames); },
      [&](
        const int threadIndex, const int numFrames) { process(threadIndex, numFrames); },
      [&](const StereoAudioBufferPtrs outputBuffer, const uint64_t hostTime,
          const int numFrames) { renderEnded(outputBuffer, hostTime, numFrames); });

    const auto chordPartials = generateChord(
      mHost->driver().sampleRate(), kAmpSmoothingDuration, kChordNoteNumbers);
    mSineBank.setPartials(randomizePhases(chordPartials, kNumUnrandomizedPhases));

    if (mHost->driver().status() != Driver::Status::kInvalid)
    {
      mHost->driver().start();
    }
  }

  AudioHost& host() { return *mHost; }

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
                           const int numFrames)
  {
    DriveMeasurement driveMeasurement{};
    driveMeasurement.hostTime = machAbsoluteTimeToSeconds(hostTime).count();
    driveMeasurement.duration =
      std::chrono::duration<double>{bufferEndTime - bufferStartTime}.count();
    driveMeasurement.numFrames = numFrames;
    std::fill_n(driveMeasurement.numActivePartialsProcessed, MAX_NUM_THREADS, -1);
    std::fill_n(driveMeasurement.cpuNumbers, MAX_NUM_THREADS, -1);
    for (int i = 0; i < mHost->numWorkerThreads() + 1; ++i)
    {
      driveMeasurement.numActivePartialsProcessed[i] = mNumActivePartialsProcessed[i];
      driveMeasurement.cpuNumbers[i] = mCpuNumbers[i];
    }
    mDriveMeasurements.tryPushBack(driveMeasurement);
  }

  // Called with no audio threads active after app launch and setting changes
  void setup(const int numWorkerThreads)
  {
    assertRelease((numWorkerThreads + 1) <= MAX_NUM_THREADS, "Too many worker threads");

    mSineBank.setNumThreads(numWorkerThreads + 1);
    for (int i = 1; i <= numWorkerThreads; ++i)
    {
      mNumActivePartialsProcessed[i] = -1;
      mCpuNumbers[i] = -1;
    }
  }

  // Called at the start of the audio I/O callback with no worker threads active
  void renderStarted(const int numFrames)
  {
    mRenderStartTime = Clock::now();

    if (const auto duration = mSineBurstDuration.exchange(0.0f))
    {
      mNumSineBurstSamplesRemaining = float(mHost->driver().sampleRate()) * duration;
    }

    const auto effectiveNumSines =
      mNumSines.load()
      + (mNumSineBurstSamplesRemaining > 0 ? mNumAdditionalSinesInBurst.load() : 0);
    mSineBank.prepare(effectiveNumSines, numFrames);

    if (!mHost->processInDriverThread())
    {
      mNumActivePartialsProcessed[0] = -1;
      mCpuNumbers[0] = cpuNumber();
    }
  }

  // Called by the main audio I/O thread and by worker audio threads
  void process(const int threadIndex, const int numFrames)
  {
    mNumActivePartialsProcessed[threadIndex] = mSineBank.process(threadIndex, numFrames);
    mCpuNumbers[threadIndex] = cpuNumber();
  }

  // Called at the end of the audio I/O callback with no worker threads active
  void renderEnded(const StereoAudioBufferPtrs outputBuffer,
                   const uint64_t hostTime,
                   const int numFrames)
  {
    mSineBank.mixTo(outputBuffer, numFrames);

    mNumSineBurstSamplesRemaining =
      std::max<int>(0, mNumSineBurstSamplesRemaining - numFrames);

    const auto endTime = Clock::now();
    addDriveMeasurement(hostTime, mRenderStartTime, endTime, numFrames);
  }

  std::optional<AudioHost> mHost;
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

- (int)preferredBufferSize { return mEngine.host().preferredBufferSize(); }
- (void)setPreferredBufferSize:(int)preferredBufferSize
{
  mEngine.host().setPreferredBufferSize(preferredBufferSize);
}

- (double)sampleRate { return mEngine.host().driver().sampleRate(); }

- (int)numWorkerThreads { return mEngine.host().numWorkerThreads(); }
- (void)setNumWorkerThreads:(int)numThreads
{
  mEngine.host().setNumWorkerThreads(numThreads);
}

- (int)numBusyThreads { return mEngine.host().numBusyThreads(); }
- (void)setNumBusyThreads:(int)numThreads
{
  mEngine.host().setNumBusyThreads(numThreads);
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
