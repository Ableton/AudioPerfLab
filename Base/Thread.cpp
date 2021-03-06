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
#include <sys/sysctl.h>
#include <system_error>

namespace
{

// See MAXTHREADNAMESIZE in the XNU sources. Includes the null terminating byte.
constexpr auto kMaxThreadNameSize = 64;

const mach_timebase_info_data_t sMachTimebaseInfo = [] {
  mach_timebase_info_data_t machTimebaseInfo;
  if (mach_timebase_info(&machTimebaseInfo) != KERN_SUCCESS)
  {
    throw std::runtime_error("could not get mach time base");
  }
  return machTimebaseInfo;
}();

} // namespace

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

std::string currentThreadName()
{
  char str[kMaxThreadNameSize] = {};
  const auto result = pthread_getname_np(pthread_self(), str, kMaxThreadNameSize);
  return result == 0 ? str : "";
}

void setCurrentThreadName(const std::string& name)
{
  // pthread_setname_np() won't set the name if it is too long, so truncate it to be
  // at most kMaxThreadNameSize characters long, including the null terminating byte.
  const std::string truncatedName{name, 0, kMaxThreadNameSize - 1};

  pthread_setname_np(truncatedName.c_str());
}

std::optional<int32_t> numPhysicalCpus()
{
  int32_t result = 0;
  size_t size = sizeof(int32_t);
  return sysctlbyname("hw.physicalcpu", &result, &size, nullptr, 0) == 0
           ? std::optional<int32_t>{result}
           : std::nullopt;
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
         "Setting time constraint policy for %s: "
         "(period: %d, computation: %d, constraint: %d)",
         currentThreadName().c_str(), policy.period, policy.computation,
         policy.constraint);

  const kern_return_t result = thread_policy_set(
    pthread_mach_thread_np(thread), THREAD_TIME_CONSTRAINT_POLICY,
    reinterpret_cast<thread_policy_t>(&policy), THREAD_TIME_CONSTRAINT_POLICY_COUNT);

  if (result != KERN_SUCCESS)
  {
    throw std::system_error(
      std::error_code(result, std::system_category()), mach_error_string(result));
  }
}
