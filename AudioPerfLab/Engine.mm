// Copyright: 2018, Ableton AG, Berlin. All rights reserved.

#import "Engine.hpp"

#include "Constants.hpp"
#include "Driver.hpp"
#include "FixedSPSCQueue.hpp"
#include "Math.hpp"
#include "ParallelSineBank.hpp"
#include "Partial.hpp"
#include "Semaphore.hpp"
#include "Thread.hpp"

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
    : mDriver{[this](const auto... xs) { return this->render(xs...); }}
  {
    setupWorkerThreads(kDefaultNumWorkerThreads);
    setupBusyThreads(kDefaultNumBusyThreads);

    if (mDriver.status() != Driver::Status::kInvalid)
    {
      mSineBank.setPartials(
        generateChord(mDriver.sampleRate(), kAmpSmoothingDuration, kChordNoteNumbers));
      mDriver.start();
    }
  }

  ~EngineImpl()
  {
    mDriver.stop();
    teardownWorkerThreads();
    teardownBusyThreads();
  }

  auto& driver() { return mDriver; }

  int preferredBufferSize() { return mDriver.preferredBufferSize(); }
  void setPreferredBufferSize(const int preferredBufferSize)
  {
    if (preferredBufferSize != mDriver.preferredBufferSize())
    {
      // Recreate the worker threads in order to use the new buffer size when setting
      // the thread policy.

      const auto numWorkerThreads = int(mWorkerThreads.size());
      mDriver.stop();
      teardownWorkerThreads();
      mDriver.setPreferredBufferSize(preferredBufferSize);
      setupWorkerThreads(numWorkerThreads);
      mDriver.start();
    }
  }

  int numWorkerThreads() const { return int(mWorkerThreads.size()); }
  void setNumWorkerThreads(const int numWorkerThreads)
  {
    if (numWorkerThreads != int(mWorkerThreads.size()))
    {
      mDriver.stop();
      teardownWorkerThreads();
      setupWorkerThreads(numWorkerThreads);
      mDriver.start();
    }
  }

  int numBusyThreads() const { return int(mBusyThreads.size()); }
  void setNumBusyThreads(const int numBusyThreads)
  {
    if (numBusyThreads != int(mBusyThreads.size()))
    {
      teardownBusyThreads();
      setupBusyThreads(numBusyThreads);
    }
  }

  bool processInDriverThread() const { return mProcessInDriverThread; }
  void setProcessInDriverThread(const bool isEnabled)
  {
    mProcessInDriverThread = isEnabled;
  }

  bool isWorkIntervalOn() const { return mIsWorkIntervalOn; }
  void setIsWorkIntervalOn(const bool isOn)
  {
    if (isOn != mIsWorkIntervalOn)
    {
      const auto numWorkerThreads = int(mWorkerThreads.size());
      mDriver.stop();
      teardownWorkerThreads();
      mIsWorkIntervalOn = isOn;
      setupWorkerThreads(numWorkerThreads);
      mDriver.start();
    }
  }

  double minimumLoad() const { return mMinimumLoad; }
  void setMinimumLoad(const double minimumLoad) { mMinimumLoad = minimumLoad; }

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
  void setupWorkerThreads(const int numWorkerThreads)
  {
    assert((numWorkerThreads + 1) <= MAX_NUM_THREADS);

    mSineBank.setNumThreads(numWorkerThreads + 1);

    mAreWorkerThreadsActive = true;
    for (int i = 1; i <= numWorkerThreads; ++i)
    {
      mWorkerThreads.emplace_back(&EngineImpl::workerThread, this, i);
      mCpuNumbers[i] = -1;
    }
  }

  void teardownWorkerThreads()
  {
    mAreWorkerThreadsActive = false;
    for (size_t i = 0; i < mWorkerThreads.size(); ++i)
    {
      mStartWorkingSemaphore.post();
    }
    for (auto& thread : mWorkerThreads)
    {
      thread.join();
    }
    mWorkerThreads.clear();
  }

  void setupBusyThreads(const int numBusyThreads)
  {
    mAreBusyThreadsActive = true;
    for (int i = 0; i < numBusyThreads; ++i)
    {
      mBusyThreads.emplace_back(&EngineImpl::busyThread, this);
    }
  }

  void teardownBusyThreads()
  {
    mAreBusyThreadsActive = false;
    for (auto& busyThread : mBusyThreads)
    {
      busyThread.join();
    }
    mBusyThreads.clear();
  }

  void ensureMinimumLoad(const std::chrono::time_point<Clock> bufferStartTime,
                         const int numFrames)
  {
    const auto bufferDuration =
      std::chrono::duration<double>{numFrames / mDriver.sampleRate()};
    hardwareDelayUntil(bufferStartTime + (bufferDuration * double(mMinimumLoad)));
  }

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
    std::fill_n(driveMeasurement.cpuNumbers, MAX_NUM_THREADS, -1);
    for (size_t i = 0; i < mWorkerThreads.size() + 1; ++i)
    {
      driveMeasurement.cpuNumbers[i] = mCpuNumbers[i];
    }
    mDriveMeasurements.tryPushBack(driveMeasurement);
  }

  void mixTo(AudioBuffer* pDestBuffers, const int numFrames)
  {
    const std::array<float*, 2> dest{static_cast<float*>(pDestBuffers[0].mData),
                                     static_cast<float*>(pDestBuffers[1].mData)};
    std::fill_n(dest[0], numFrames, 0.0f);
    std::fill_n(dest[1], numFrames, 0.0f);
    mSineBank.mixTo(dest, numFrames);
  }

  OSStatus render(AudioUnitRenderActionFlags* ioActionFlags,
                  const AudioTimeStamp* inTimeStamp,
                  UInt32 inBusNumber,
                  UInt32 inNumberFrames,
                  AudioBufferList* ioData)
  {
    const auto startTime = Clock::now();
    mNumFrames = inNumberFrames;

    if (const auto duration = mSineBurstDuration.exchange(0.0f))
    {
      mNumSineBurstSamplesRemaining = float(mDriver.sampleRate()) * duration;
    }

    const auto effectiveNumSines =
      mNumSines.load()
      + (mNumSineBurstSamplesRemaining > 0 ? mNumAdditionalSinesInBurst.load() : 0);
    mSineBank.prepare(effectiveNumSines, inNumberFrames);

    for (size_t i = 0; i < mWorkerThreads.size(); ++i)
    {
      mStartWorkingSemaphore.post();
    }

    if (mProcessInDriverThread)
    {
      mSineBank.process(0, inNumberFrames);
    }
    mCpuNumbers[0] = cpuNumber();

    for (size_t i = 0; i < mWorkerThreads.size(); ++i)
    {
      mFinishedWorkSemaphore.wait();
    }

    mixTo(ioData->mBuffers, inNumberFrames);
    mNumSineBurstSamplesRemaining =
      std::max<int>(0, mNumSineBurstSamplesRemaining - inNumberFrames);

    const auto endTime = Clock::now();
    addDriveMeasurement(inTimeStamp->mHostTime, startTime, endTime, inNumberFrames);

    if (mProcessInDriverThread)
    {
      ensureMinimumLoad(startTime, inNumberFrames);
    }

    return noErr;
  }

  void workerThread(const int threadIndex)
  {
    setThreadTimeConstraintPolicy(
      pthread_self(),
      TimeConstraintPolicy{mDriver.nominalBufferDuration(), kRealtimeThreadQuantum,
                           mDriver.nominalBufferDuration()});

    bool needToJoinWorkInterval = mIsWorkIntervalOn;
    while (1)
    {
      mStartWorkingSemaphore.wait();
      if (!mAreWorkerThreadsActive)
      {
        break;
      }

      // Join after waking from the semaphore to ensure that the CoreAudio thread is
      // active so that findAndJoinWorkInterval() can find its work interval.
      if (needToJoinWorkInterval)
      {
        findAndJoinWorkInterval();
        needToJoinWorkInterval = false;
      }

      const auto startTime = Clock::now();
      const auto numFrames = mNumFrames.load();
      mSineBank.process(threadIndex, numFrames);
      mCpuNumbers[threadIndex] = cpuNumber();
      mFinishedWorkSemaphore.post();
      ensureMinimumLoad(startTime, numFrames);
    }

    if (mIsWorkIntervalOn)
    {
      leaveWorkInterval();
    }
  }

  // A low-priority thread that yields for 90% of the time and sleeps for the rest.
  void busyThread()
  {
    sched_param param{};
    param.sched_priority = sched_get_priority_min(SCHED_FIFO);
    pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);

    while (mAreBusyThreadsActive)
    {
      const auto delayUntilTime = Clock::now() + std::chrono::milliseconds{9};
      hardwareDelayUntil(delayUntilTime);
      std::this_thread::sleep_until(delayUntilTime + std::chrono::milliseconds{1});
    }
  }

  Driver mDriver;
  ParallelSineBank mSineBank;

  std::atomic<bool> mProcessInDriverThread{true};
  bool mIsWorkIntervalOn{false};
  std::atomic<int> mNumFrames{0};
  std::atomic<int> mNumSines{kDefaultNumSines};

  std::atomic<int> mNumAdditionalSinesInBurst{0};
  std::atomic<float> mSineBurstDuration{0.0f};
  int mNumSineBurstSamplesRemaining{0};

  std::atomic<bool> mAreWorkerThreadsActive{false};
  std::vector<std::thread> mWorkerThreads;
  std::array<std::atomic<int>, MAX_NUM_THREADS> mCpuNumbers;

  std::atomic<bool> mAreBusyThreadsActive{false};
  std::vector<std::thread> mBusyThreads;

  std::atomic<double> mMinimumLoad{0.0};
  Semaphore mStartWorkingSemaphore{0};
  Semaphore mFinishedWorkSemaphore{0};
  FixedSPSCQueue<DriveMeasurement> mDriveMeasurements{kDriveMeasurementQueueSize};
};

@implementation Engine
{
  EngineImpl mEngine;
}

- (int)preferredBufferSize { return mEngine.preferredBufferSize(); }
- (void)setPreferredBufferSize:(int)preferredBufferSize
{
  mEngine.setPreferredBufferSize(preferredBufferSize);
}

- (double)sampleRate { return mEngine.driver().sampleRate(); }

- (int)numWorkerThreads { return mEngine.numWorkerThreads(); }
- (void)setNumWorkerThreads:(int)numThreads { mEngine.setNumWorkerThreads(numThreads); }

- (int)numBusyThreads { return mEngine.numBusyThreads(); }
- (void)setNumBusyThreads:(int)numThreads { mEngine.setNumBusyThreads(numThreads); }

- (bool)processInDriverThread { return mEngine.processInDriverThread(); }
- (void)setProcessInDriverThread:(bool)enabled
{
  mEngine.setProcessInDriverThread(enabled);
}

- (bool)isWorkIntervalOn { return mEngine.isWorkIntervalOn(); }
- (void)setIsWorkIntervalOn:(bool)isOn { mEngine.setIsWorkIntervalOn(isOn); }

- (double)minimumLoad { return mEngine.minimumLoad(); }
- (void)setMinimumLoad:(double)minimumLoad { mEngine.setMinimumLoad(minimumLoad); }

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
