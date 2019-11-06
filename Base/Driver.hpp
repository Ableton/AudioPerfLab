// Copyright: 2018, Ableton AG, Berlin. All rights reserved.

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
