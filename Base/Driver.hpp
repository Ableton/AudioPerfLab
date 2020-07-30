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

#include "Config.hpp"
#include "FixedSPSCQueue.hpp"
#include "VolumeFader.hpp"

#include <AudioToolbox/AUComponent.h>
#include <CoreAudio/CoreAudioTypes.h>
#include <chrono>
#include <functional>
#include <mutex>

class Driver
{
public:
  using Seconds = std::chrono::duration<double>;

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

  Status status() const;

  double sampleRate() const;
  Seconds nominalBufferDuration() const;

  int preferredBufferSize() const;
  void setPreferredBufferSize(int preferredBufferSize);

  /*! Enable or disable audio input.
   *
   * Input is disabled by default. Note that this is an expensive operation that can block
   * for up to half a second.
   */
  bool isInputEnabled() const;
  void setIsInputEnabled(bool isInputEnabled);

  //! The volume of the output is an amplitude and must be >= 0
  float outputVolume() const;
  void setOutputVolume(float volume, Seconds fadeDuration);

private:
  struct FadeCommand
  {
    void operator()(Driver& driver) const
    {
      driver.mVolumeFader.fadeTo(targetOutputVolume, numFrames);
    }

    float targetOutputVolume{};
    uint64_t numFrames{};
  };

  void requestBufferSize(int requestedBufferSize);

  void setupAudioSession();
  void teardownAudioSession();

  void setupIoUnit();
  void teardownIoUnit();

  OSStatus render(AudioUnitRenderActionFlags* ioActionFlags,
                  const AudioTimeStamp* inTimeStamp,
                  UInt32 inBusNumber,
                  UInt32 inNumberFrames,
                  AudioBufferList* ioData);

  AudioUnit mpRemoteIoUnit{};
  FixedSPSCQueue<FadeCommand> mCommandQueue;

  bool mIsInputEnabled{false};
  int mPreferredBufferSize{kDefaultPreferredBufferSize};
  double mSampleRate{-1.0};
  Seconds mNominalBufferDuration{-1.0};
  Status mStatus{Status::kStopped};

  VolumeFader<float> mVolumeFader;
  float mOutputVolume{1.0};

  RenderCallback mRenderCallback;
  std::mutex mRenderMutex;
  std::unique_lock<std::mutex> mRenderLock;
};
