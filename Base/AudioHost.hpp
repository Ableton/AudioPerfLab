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

#pragma once

#include "AudioBuffer.hpp"
#include "Config.hpp"
#include "Driver.hpp"
#include "Semaphore.hpp"

#include <CoreAudio/CoreAudioTypes.h>
#include <array>
#include <atomic>
#include <chrono>
#include <functional>
#include <optional>
#include <thread>
#include <vector>

/*! An AudioHost contains an audio driver and a set of worker threads, and invokes
 * the callbacks taken by the constructor to process audio and measurements. It provides
 * various means of of getting optimal performance out of audio threads which can be
 * controlled via the setters.
 */
class AudioHost
{
  using Clock = std::chrono::high_resolution_clock;

public:
  using Setup = std::function<void(int numWorkerThreads)>;
  using RenderStarted =
    std::function<void(StereoAudioBufferPtrs ioBuffer, int numFrames)>;
  using Process = std::function<void(int threadIndex, int numFrames)>;
  using RenderEnded =
    std::function<void(StereoAudioBufferPtrs ioBuffer, uint64_t hostTime, int numFrames)>;

  AudioHost(Setup setup,
            RenderStarted renderStarted,
            Process process,
            RenderEnded renderEnded);
  ~AudioHost();

  Driver& driver();
  const Driver& driver() const;

  void start();
  void stop();

  bool isAudioInputEnabled() const;
  void setIsAudioInputEnabled(bool isInputEnabled);

  int preferredBufferSize() const;
  void setPreferredBufferSize(const int preferredBufferSize);

  int numWorkerThreads() const;
  void setNumWorkerThreads(int numWorkerThreads);

  bool processInDriverThread() const;
  void setProcessInDriverThread(bool isEnabled);

  bool isWorkIntervalOn() const;
  void setIsWorkIntervalOn(bool isOn);

  double minimumLoad() const;
  void setMinimumLoad(double minimumLoad);

private:
  void whileStopped(const std::function<void()>& f);

  void setupDriver(Driver::Config config);
  void teardownDriver();

  void setupWorkerThreads();
  void teardownWorkerThreads();

  void ensureMinimumLoad(std::chrono::time_point<Clock> bufferStartTime, int numFrames);

  OSStatus render(AudioUnitRenderActionFlags* ioActionFlags,
                  const AudioTimeStamp* inTimeStamp,
                  UInt32 inBusNumber,
                  UInt32 inNumberFrames,
                  AudioBufferList* ioData);

  void workerThread(int threadIndex);

  std::optional<Driver> mDriver;

  std::atomic<bool> mProcessInDriverThread{true};
  bool mIsWorkIntervalOn{false};
  std::atomic<int> mNumFrames{0};

  std::atomic<bool> mAreWorkerThreadsActive{false};
  std::vector<std::thread> mWorkerThreads;

  std::atomic<double> mMinimumLoad{0.0};
  Semaphore mStartWorkingSemaphore{0};
  Semaphore mFinishedWorkSemaphore{0};

  Setup mSetup;
  RenderStarted mRenderStarted;
  Process mProcess;
  RenderEnded mRenderEnded;

  bool mIsStarted{false};
  int mNumRequestedWorkerThreads{kDefaultNumWorkerThreads};
};
