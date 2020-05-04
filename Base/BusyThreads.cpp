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

#include "BusyThreads.hpp"

#include "Assert.hpp"
#include "Thread.hpp"

#include <chrono>
#include <pthread.h>
#include <pthread/sched.h>
#include <string>

BusyThreads::BusyThreads() { setup(kDefaultNumBusyThreads); }
BusyThreads::~BusyThreads() { teardown(); }

int BusyThreads::numThreads() const { return int(mThreads.size()); }
void BusyThreads::setNumThreads(const int numThreads)
{
  if (numThreads != int(mThreads.size()))
  {
    teardown();
    setup(numThreads);
  }
}

void BusyThreads::setup(const int numThreads)
{
  assertRelease(!mIsActive && mThreads.empty(),
                "Busy threads must be torn down before calling setup()");

  mIsActive = true;
  for (int i = 0; i < numThreads; ++i)
  {
    mThreads.emplace_back(&BusyThreads::busyThread, this, i);
  }
}

void BusyThreads::teardown()
{
  mIsActive = false;
  for (auto& thread : mThreads)
  {
    thread.join();
  }
  mThreads.clear();
}

void BusyThreads::busyThread(const int threadIndex)
{
  using Clock = std::chrono::high_resolution_clock;

  constexpr auto kLowEnergyDelayDuration = std::chrono::milliseconds{10};
  constexpr auto kSleepDuration = std::chrono::milliseconds{5};

  setCurrentThreadName("Busy Thread " + std::to_string(threadIndex + 1));

  sched_param param{};
  param.sched_priority = sched_get_priority_min(SCHED_OTHER);
  pthread_setschedparam(pthread_self(), SCHED_OTHER, &param);

  while (mIsActive)
  {
    const auto delayUntilTime = Clock::now() + kLowEnergyDelayDuration;
    hardwareDelayUntil(delayUntilTime);

    // Sleep to avoid being terminated when running in the background by violating the
    // iOS CPU usage limit
    std::this_thread::sleep_until(delayUntilTime + kSleepDuration);
  }
}
