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

#include <AudioToolbox/AUComponent.h>
#include <CoreAudio/CoreAudioTypes.h>
#include <chrono>
#include <functional>
#include <mutex>

class Driver
{
public:
  enum class Status
  {
    kStopped,
    kStarted,
    kInvalid,
  };

  using RenderCallback = std::function<OSStatus(AudioUnitRenderActionFlags* ioActionFlags,
                                                const AudioTimeStamp* inTimeStamp,
                                                UInt32 inBusNumber,
                                                UInt32 inNumberFrames,
                                                AudioBufferList* ioData)>;

  explicit Driver(RenderCallback renderCallback);
  ~Driver();

  void start();
  void stop();

  int preferredBufferSize() const;
  void setPreferredBufferSize(int preferredBufferSize);

  std::chrono::duration<double> nominalBufferDuration() const;
  double sampleRate() const;
  Status status() const;

private:
  void setupAudioSession();
  void teardownAudioSession();

  void setupIoUnit();
  void teardownIoUnit();

  AudioUnit mRemoteIoUnit{};
  int mPreferredBufferSize{-1};
  double mSampleRate{-1.0};
  std::chrono::duration<double> mNominalBufferDuration{-1.0};
  Status mStatus{Status::kStopped};
  RenderCallback mRenderCallback;
  std::mutex mRenderMutex;
  std::unique_lock<std::mutex> mRenderLock;
};
