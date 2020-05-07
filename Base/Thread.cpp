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

#include "Thread.hpp"

#include <mach/mach.h>
#include <mach/mach_init.h>
#include <mach/mach_time.h>
#include <mach/thread_act.h>
#include <mach/thread_policy.h>
#include <os/log.h>
#include <system_error>

extern "C" {
int work_interval_join_port(mach_port_t port);
int work_interval_leave();
}

static const mach_timebase_info_data_t sMachTimebaseInfo = [] {
  mach_timebase_info_data_t machTimebaseInfo;
  if (mach_timebase_info(&machTimebaseInfo) != KERN_SUCCESS)
  {
    throw std::runtime_error("could not get mach time base");
  }
  return machTimebaseInfo;
}();

uint64_t secondsToMachAbsoluteTime(const std::chrono::duration<double> duration)
{
  const auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
  return uint64_t(nanoseconds.count() * sMachTimebaseInfo.denom
                  / sMachTimebaseInfo.numer);
}

std::chrono::duration<double> machAbsoluteTimeToSeconds(const uint64_t machAbsoluteTime)
{
  return std::chrono::nanoseconds{machAbsoluteTime * sMachTimebaseInfo.numer
                                  / sMachTimebaseInfo.denom};
}

void setCurrentThreadName(const std::string& name)
{
  pthread_setname_np(name.c_str());
}

void setThreadTimeConstraintPolicy(const pthread_t thread,
                                   const TimeConstraintPolicy& timeConstraintPolicy)
{
  thread_time_constraint_policy policy{};
  policy.period = uint32_t(secondsToMachAbsoluteTime(timeConstraintPolicy.period));
  policy.computation = uint32_t(secondsToMachAbsoluteTime(timeConstraintPolicy.quantum));
  policy.constraint =
    uint32_t(secondsToMachAbsoluteTime(timeConstraintPolicy.constraint));
  policy.preemptible = 1;

  os_log(OS_LOG_DEFAULT,
         "Setting time constraint policy: (period: %d, computation: %d, constraint: %d)",
         policy.period, policy.computation, policy.constraint);

  const kern_return_t result = thread_policy_set(
    pthread_mach_thread_np(thread), THREAD_TIME_CONSTRAINT_POLICY,
    reinterpret_cast<thread_policy_t>(&policy), THREAD_TIME_CONSTRAINT_POLICY_COUNT);

  if (result != KERN_SUCCESS)
  {
    throw std::system_error(
      std::error_code(result, std::system_category()), mach_error_string(result));
  }
}

void findAndJoinWorkInterval()
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

void leaveWorkInterval()
{
  if (work_interval_leave() == 0)
  {
    os_log(OS_LOG_DEFAULT, "Left work interval");
  }
  else
  {
    throw std::runtime_error("Couldn't leave work interval");
  }
}
