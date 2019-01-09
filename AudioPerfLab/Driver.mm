// Copyright: 2018, Ableton AG, Berlin. All rights reserved.

#include "Driver.hpp"

#include "Constants.hpp"

#import <AVFoundation/AVAudioSession.h>
#include <AudioToolbox/AudioToolbox.h>
#include <algorithm>
#include <os/log.h>
#include <sstream>
#include <string>

namespace
{

std::string errorCodeString(const OSStatus errorCode)
{
  std::stringstream errorCodeStream;

  char printableCode[5] = {};
  *reinterpret_cast<UInt32*>(printableCode) = CFSwapInt32HostToBig(errorCode);
  if (std::all_of(printableCode, printableCode + 4,
                  [](const char c) { return std::isprint(c) != 0; }))
  {
    errorCodeStream << "'" << printableCode << "'";
  }
  else if (errorCode > -200000 && errorCode < 200000)
  {
    errorCodeStream << int(errorCode);
  }
  else
  {
    errorCodeStream << std::hex << int(errorCode);
  }

  return errorCodeStream.str();
}

std::string errorDescription(const OSStatus errorCode, const std::string& errorString)
{
  return errorString + " (error code: " + errorCodeString(errorCode) + ")";
}

void throwIfError(const OSStatus errorCode, const std::string& errorString)
{
  if (errorCode != noErr)
  {
    throw std::runtime_error(errorDescription(errorCode, errorString));
  }
}

} // anonymous namespace

Driver::Driver(Driver::RenderCallback renderCallback)
  : mRenderCallback{[callback = std::move(renderCallback), this](const auto... xs) {
    // Using a lock to stop audio instead of AudioOutputUnitStart()/AudioOutputUnitStop()
    // is faster and avoids TSan errors.
    std::unique_lock<std::mutex> lock(mRenderMutex, std::try_to_lock);
    if (lock.owns_lock())
    {
      return callback(xs...);
    }

    return OSStatus(noErr);
  }}
  , mRenderLock{mRenderMutex}
{
  try
  {
    setupAudioSession();
    setupIoUnit();
    os_log(OS_LOG_DEFAULT, "Sample Rate: %.0f", mSampleRate);
  }
  catch (const std::runtime_error& exception)
  {
    os_log_error(OS_LOG_DEFAULT, "%s", exception.what());
    mStatus = Status::kInvalid;
  }
}

Driver::~Driver()
{
  try
  {
    teardownIoUnit();
    teardownAudioSession();
  }
  catch (const std::runtime_error& exception)
  {
    os_log_error(OS_LOG_DEFAULT, "%s", exception.what());
    mStatus = Status::kInvalid;
  }
}

void Driver::start()
{
  if (mStatus == Status::kStopped)
  {
    mRenderLock = {};
    mStatus = Status::kStarted;
  }
}

void Driver::stop()
{
  if (mStatus == Status::kStarted)
  {
    mRenderLock = std::unique_lock<std::mutex>{mRenderMutex};
    mStatus = Status::kStopped;
  }
}

int Driver::preferredBufferSize() const { return mPreferredBufferSize; }
void Driver::setPreferredBufferSize(const int preferredBufferSize)
{
  if (preferredBufferSize != mPreferredBufferSize)
  {
    AVAudioSession* audioSession = AVAudioSession.sharedInstance;

    const NSTimeInterval bufferDuration = preferredBufferSize / audioSession.sampleRate;

    NSError* error = nil;
    [audioSession setPreferredIOBufferDuration:bufferDuration error:&error];
    if (error.code != noErr)
    {
      const auto errorDesc =
        errorDescription(OSStatus(error.code), "couldn't set the I/O buffer duration");
      os_log_error(OS_LOG_DEFAULT, "%s", errorDesc.c_str());
    }

    mNominalBufferDuration = std::chrono::duration<double>{audioSession.IOBufferDuration};
    mPreferredBufferSize = preferredBufferSize;
  }
}

std::chrono::duration<double> Driver::nominalBufferDuration() const
{
  // AVAudioSession.IOBufferDuration sends mach messages, so return a cached value for use
  // in real-time.
  return mNominalBufferDuration;
}

double Driver::sampleRate() const
{
  // AVAudioSession.sampleRate sends mach messages, so return a cached value for use in
  // real-time.
  return mSampleRate;
}

Driver::Status Driver::status() const { return mStatus; }

void Driver::setupAudioSession()
{
  AVAudioSession* audioSession = AVAudioSession.sharedInstance;

  NSError* error = nil;
  [audioSession setCategory:AVAudioSessionCategoryPlayback
                withOptions:AVAudioSessionCategoryOptionMixWithOthers
                      error:&error];
  throwIfError(OSStatus(error.code), "couldn't set session's audio category");

  mSampleRate = AVAudioSession.sharedInstance.sampleRate;
  setPreferredBufferSize(kDefaultPreferredBufferSize);

  [AVAudioSession.sharedInstance setActive:YES error:&error];
  throwIfError(OSStatus(error.code), "couldn't set session active");
}

void Driver::teardownAudioSession()
{
  AVAudioSession* audioSession = AVAudioSession.sharedInstance;

  NSError* error = nil;
  [audioSession setActive:NO error:&error];
  throwIfError(OSStatus(error.code), "couldn't deactivate session");
}

void Driver::setupIoUnit()
{
  AudioComponentDescription desc{};
  desc.componentType = kAudioUnitType_Output;
  desc.componentSubType = kAudioUnitSubType_RemoteIO;
  desc.componentManufacturer = kAudioUnitManufacturer_Apple;
  desc.componentFlags = 0;
  desc.componentFlagsMask = 0;

  AudioComponent comp = AudioComponentFindNext(nullptr, &desc);
  throwIfError(AudioComponentInstanceNew(comp, &mRemoteIoUnit),
               "couldn't create a new instance of AURemoteIO");

  AudioStreamBasicDescription streamDescription{};
  streamDescription.mSampleRate = AVAudioSession.sharedInstance.sampleRate;
  streamDescription.mFormatID = kAudioFormatLinearPCM;
  streamDescription.mFormatFlags =
    kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsPacked
    | kAudioFormatFlagIsNonInterleaved | kAudioFormatFlagIsFloat;
  const auto kBytesPerSample = sizeof(float);
  streamDescription.mBytesPerPacket = kBytesPerSample;
  streamDescription.mFramesPerPacket = 1;
  streamDescription.mBytesPerFrame = kBytesPerSample;
  streamDescription.mChannelsPerFrame = 2;
  streamDescription.mBitsPerChannel = kBytesPerSample * 8;
  throwIfError(AudioUnitSetProperty(mRemoteIoUnit, kAudioUnitProperty_StreamFormat,
                                    kAudioUnitScope_Input, 0, &streamDescription,
                                    sizeof(streamDescription)),
               "couldn't set the format on AURemoteIO");

  AURenderCallbackStruct renderCallback{};
  renderCallback.inputProc = [](void* inRefCon, AudioUnitRenderActionFlags* ioActionFlags,
                                const AudioTimeStamp* inTimeStamp, UInt32 inBusNumber,
                                UInt32 inNumberFrames,
                                AudioBufferList* ioData) -> OSStatus {
    auto* self = static_cast<Driver*>(inRefCon);
    return self->mRenderCallback(
      ioActionFlags, inTimeStamp, inBusNumber, inNumberFrames, ioData);
  };
  renderCallback.inputProcRefCon = this;
  throwIfError(AudioUnitSetProperty(mRemoteIoUnit, kAudioUnitProperty_SetRenderCallback,
                                    kAudioUnitScope_Input, 0, &renderCallback,
                                    sizeof(renderCallback)),
               "couldn't set render callback on AURemoteIO");

  throwIfError(
    AudioUnitInitialize(mRemoteIoUnit), "couldn't initialize AURemoteIO instance");

  throwIfError(AudioOutputUnitStart(mRemoteIoUnit), "couldn't start output unit");
}

void Driver::teardownIoUnit()
{
  if (mRemoteIoUnit)
  {
    throwIfError(AudioOutputUnitStop(mRemoteIoUnit), "couldn't stop output unit");
    throwIfError(AudioUnitUninitialize(mRemoteIoUnit),
                 "couldn't uninitialize the AURemoteIO instance");

    // TODO: ensure that the instance is disposed of even after an error above
    throwIfError(AudioComponentInstanceDispose(mRemoteIoUnit),
                 "couldn't dispose of the AURemoteIO instance");
  }
}
