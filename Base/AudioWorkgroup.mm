/*
 * Copyright (c) 2020 Ableton AG, Berlin
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

#include "AudioWorkgroup.hpp"

#include "Base/Assert.hpp"

#include <exception>
#include <mach/mach.h>
#include <mach/mach_init.h>
#include <os/log.h>
#include <system_error>

/* Note that special member functions in AudioWorkgroup and
 * AudioWorkgroup::ScopedMembership must be explicitly defined (e.g., using "= default")
 * in this source file so that ARC retains and releases are present even if the header is
 * included in a C++ TU. Additionally, this file must be Objective-C++.
 */

AudioWorkgroup::ScopedMembership::ScopedMembership(const os_workgroup_t pWorkgroup)
  : mpWorkgroup{pWorkgroup}
{
  const auto result = os_workgroup_join(mpWorkgroup, &mJoinToken);
  if (result != 0)
  {
    throw std::runtime_error("Couldn't join the workgroup");
  }
}

AudioWorkgroup::ScopedMembership::ScopedMembership(
  AudioWorkgroup::ScopedMembership&& other)
  : mpWorkgroup{std::exchange(other.mpWorkgroup, nullptr)}
  , mJoinToken{other.mJoinToken}
{
}

AudioWorkgroup::ScopedMembership& AudioWorkgroup::ScopedMembership::operator=(
  AudioWorkgroup::ScopedMembership&& rhs)
{
  if (this != &rhs)
  {
    mpWorkgroup = std::exchange(rhs.mpWorkgroup, nullptr);
    mJoinToken = rhs.mJoinToken;
  }
  return *this;
}

AudioWorkgroup::ScopedMembership::~ScopedMembership()
{
  if (@available(iOS 14, *))
  {
    if (mpWorkgroup)
    {
      os_workgroup_leave(mpWorkgroup, &mJoinToken);
    }
  }
  else
  {
    fatalError("AudioWorkgroup used prior to iOS 14");
  }
}

AudioWorkgroup::AudioWorkgroup(const os_workgroup_t pWorkgroup)
  : mpWorkgroup{pWorkgroup}
{
  assertRelease(pWorkgroup != nullptr, "nullptr workgroup");
}

AudioWorkgroup::AudioWorkgroup(const AudioWorkgroup&) = default;
AudioWorkgroup& AudioWorkgroup::operator=(const AudioWorkgroup&) = default;
AudioWorkgroup::~AudioWorkgroup() = default;

int AudioWorkgroup::maxNumParallelThreads() const
{
  if (@available(iOS 14, *))
  {
    // Return zero for a moved-from AudioWorkgroup so that the object is in a valid state
    return mpWorkgroup ? os_workgroup_max_parallel_threads(mpWorkgroup, nullptr) : 0;
  }
  else
  {
    fatalError("AudioWorkgroup used prior to iOS 14");
  }
}

AudioWorkgroup::ScopedMembership AudioWorkgroup::join()
{
  return ScopedMembership{mpWorkgroup};
}


extern "C" {
int work_interval_join_port(mach_port_t port);
int work_interval_leave();
int pthread_time_constraint_max_parallelism(unsigned long flags);
}

LegacyAudioWorkgroup::ScopedMembership::ScopedMembership()
  : mIsActive{true}
{
  mach_port_name_array_t rightNames{};
  mach_msg_type_number_t rightNamesCount{};
  mach_port_type_array_t rightTypes{};
  mach_msg_type_number_t rightTypesCount{};

  const kern_return_t result = mach_port_names(
    mach_task_self(), &rightNames, &rightNamesCount, &rightTypes, &rightTypesCount);
  if (result != KERN_SUCCESS)
  {
    throw std::system_error(
      std::error_code(result, std::system_category()), mach_error_string(result));
  }
  else if (rightNamesCount != rightTypesCount)
  {
    throw std::runtime_error("Right names/right types have mismatched sizes");
  }

  for (size_t i = 0; i < rightNamesCount; ++i)
  {
    if (rightTypes[i] & MACH_PORT_TYPE_SEND)
    {
      const mach_port_t port = rightNames[i];
      if (work_interval_join_port(port) == 0)
      {
        os_log(OS_LOG_DEFAULT, "Joined work interval port %04X", port);
        return;
      }
    }
  }

  throw std::runtime_error("Couldn't find work interval");
}

LegacyAudioWorkgroup::ScopedMembership::ScopedMembership(ScopedMembership&& other)
  : mIsActive{std::exchange(other.mIsActive, false)}
{
}

LegacyAudioWorkgroup::ScopedMembership& LegacyAudioWorkgroup::ScopedMembership::operator=(
  ScopedMembership&& rhs)
{
  if (this != &rhs)
  {
    mIsActive = std::exchange(rhs.mIsActive, false);
  }
  return *this;
}

LegacyAudioWorkgroup::ScopedMembership::~ScopedMembership()
{
  if (mIsActive)
  {
    if (work_interval_leave() == 0)
    {
      os_log(OS_LOG_DEFAULT, "Left work interval");
    }
    else
    {
      fatalError("Couldn't leave the work interval");
    }
  }
}

int LegacyAudioWorkgroup::maxNumParallelThreads() const
{
  // Pass 0 rather than PTHREAD_MAX_PARALLELISM_PHYSICAL to request the logical number
  // of cores (including hyperthreading cores). This makes LegacyAudioWorkgroup and
  // AudioWorkgroup return the same value for maxNumParallelThreads() when running in the
  // simulator on x86.
  return pthread_time_constraint_max_parallelism(0);
}

LegacyAudioWorkgroup::ScopedMembership LegacyAudioWorkgroup::join()
{
  return ScopedMembership{};
}


SomeAudioWorkgroup::SomeAudioWorkgroup(const WorkgroupVariant& workgroup)
  : mWorkgroup{workgroup}
{
}

int SomeAudioWorkgroup::maxNumParallelThreads() const
{
  return std::visit(
    [](const auto& workgroup) { return workgroup.maxNumParallelThreads(); }, mWorkgroup);
}

SomeAudioWorkgroup::ScopedMembership SomeAudioWorkgroup::join()
{
  return std::visit(
    [&](auto& workgroup) { return ScopedMembership{workgroup.join()}; }, mWorkgroup);
}
