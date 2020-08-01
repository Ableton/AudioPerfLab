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

#include "AudioHost.hpp"

#include "Assert.hpp"
#include "Thread.hpp"

#include <string>
#include <utility>

AudioHost::AudioHost(Setup setup,
                     RenderStarted renderStarted,
                     Process process,
                     RenderEnded renderEnded)
  : mDriver{std::in_place, [this](const auto... xs) { return this->render(xs...); },
            Driver::Config{}}
  , mSetup{std::move(setup)}
  , mRenderStarted{std::move(renderStarted)}
  , mProcess{std::move(process)}
  , mRenderEnded{std::move(renderEnded)}
{
}

AudioHost::~AudioHost() { stop(); }

Driver& AudioHost::driver() { return *mDriver; }
const Driver& AudioHost::driver() const { return *mDriver; }

void AudioHost::start()
{
  if (!mIsStarted)
  {
    mSetup(mNumRequestedWorkerThreads);

    setupWorkerThreads();
    driver().start();
    mIsStarted = true;
  }
}

void AudioHost::stop()
{
  if (mIsStarted)
  {
    driver().stop();
    teardownWorkerThreads();
    mIsStarted = false;
  }
}

int AudioHost::preferredBufferSize() const { return driver().preferredBufferSize(); }
void AudioHost::setPreferredBufferSize(const int preferredBufferSize)
{
  if (preferredBufferSize != driver().preferredBufferSize())
  {
    // Recreate the worker threads in order to use the new buffer size when setting
    // the thread policy.
    whileStopped([&] { driver().setPreferredBufferSize(preferredBufferSize); });
  }
}

int AudioHost::numWorkerThreads() const { return int(mWorkerThreads.size()); }
void AudioHost::setNumWorkerThreads(const int numWorkerThreads)
{
  if (numWorkerThreads != mNumRequestedWorkerThreads)
  {
    whileStopped([&] { mNumRequestedWorkerThreads = numWorkerThreads; });
  }
}

bool AudioHost::processInDriverThread() const { return mProcessInDriverThread; }
void AudioHost::setProcessInDriverThread(const bool isEnabled)
{
  mProcessInDriverThread = isEnabled;
}

bool AudioHost::isWorkIntervalOn() const { return mIsWorkIntervalOn; }
void AudioHost::setIsWorkIntervalOn(const bool isOn)
{
  if (isOn != mIsWorkIntervalOn)
  {
    whileStopped([&] { mIsWorkIntervalOn = isOn; });
  }
}

double AudioHost::minimumLoad() const { return mMinimumLoad; }
void AudioHost::setMinimumLoad(const double minimumLoad) { mMinimumLoad = minimumLoad; }

void AudioHost::whileStopped(const std::function<void()>& f)
{
  const bool wasStarted = mIsStarted;
  if (wasStarted)
  {
    stop();
  }

  f();

  if (wasStarted)
  {
    start();
  }
}

void AudioHost::setupWorkerThreads()
{
  assertRelease(mWorkerThreads.empty(),
                "Worker threads must be torn down before calling setupWorkerThreads()");

  mAreWorkerThreadsActive = true;
  for (int i = 1; i <= mNumRequestedWorkerThreads; ++i)
  {
    mWorkerThreads.emplace_back(&AudioHost::workerThread, this, i);
  }
}

void AudioHost::teardownWorkerThreads()
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

void AudioHost::ensureMinimumLoad(const std::chrono::time_point<Clock> bufferStartTime,
                                  const int numFrames)
{
  const auto bufferDuration =
    std::chrono::duration<double>{numFrames / driver().sampleRate()};
  lowEnergyWorkUntil(bufferStartTime + (bufferDuration * double(mMinimumLoad)));
}

OSStatus AudioHost::render(AudioUnitRenderActionFlags* ioActionFlags,
                           const AudioTimeStamp* inTimeStamp,
                           UInt32 inBusNumber,
                           UInt32 inNumberFrames,
                           AudioBufferList* ioData)
{
  const auto startTime = Clock::now();
  mNumFrames = inNumberFrames;

  const AudioBuffer* pIoBuffers = ioData->mBuffers;
  const StereoAudioBufferPtrs ioBuffer{
    static_cast<float*>(pIoBuffers[0].mData), static_cast<float*>(pIoBuffers[1].mData)};

  mRenderStarted(ioBuffer, inNumberFrames);

  for (size_t i = 0; i < mWorkerThreads.size(); ++i)
  {
    mStartWorkingSemaphore.post();
  }

  if (mProcessInDriverThread)
  {
    mProcess(0, inNumberFrames);
  }

  for (size_t i = 0; i < mWorkerThreads.size(); ++i)
  {
    mFinishedWorkSemaphore.wait();
  }

  mRenderEnded(ioBuffer, inTimeStamp->mHostTime, inNumberFrames);

  if (mProcessInDriverThread)
  {
    ensureMinimumLoad(startTime, inNumberFrames);
  }

  return noErr;
}

void AudioHost::workerThread(const int threadIndex)
{
  setCurrentThreadName("Audio Worker Thread " + std::to_string(threadIndex));
  setThreadTimeConstraintPolicy(
    pthread_self(),
    TimeConstraintPolicy{driver().nominalBufferDuration(), kRealtimeThreadQuantum,
                         driver().nominalBufferDuration()});

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
    mProcess(threadIndex, numFrames);
    mFinishedWorkSemaphore.post();
    ensureMinimumLoad(startTime, numFrames);
  }

  if (mIsWorkIntervalOn)
  {
    leaveWorkInterval();
  }
}
