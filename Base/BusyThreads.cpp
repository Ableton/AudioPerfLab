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

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <pthread.h>
#include <pthread/sched.h>
#include <thread>

class BusyThreadImpl
{
public:
  using Seconds = BusyThread::Seconds;

  BusyThreadImpl(std::string threadName, const Seconds period, const double cpuUsage)
    : mThreadName{std::move(threadName)}
    , mPeriod{period}
    , mCpuUsage{cpuUsage}
    , mIsActive{true}
  {
    assertRelease(period > Seconds{0.0}, "Invalid busy thread period");
    assertRelease(cpuUsage >= 0.0 && cpuUsage <= 1.0, "Invalid busy thread CPU usage");

    mThread = std::thread{&BusyThreadImpl::busyThread, this};
  }

  ~BusyThreadImpl()
  {
    {
      std::unique_lock lock{mMutex};
      mIsActive.store(false, std::memory_order_release);
      mConditionVariable.notify_all();
    }

    mThread.join();
  }

private:
  void busyThread()
  {
    // A busy thread alternates between waiting on a condition variable and performing
    // low-energy work via a hardware delay instruction. Waiting avoids being terminated
    // when running in the background by violating the iOS CPU usage limit. The use of
    // mIsActive and the condition variable allows the thread to be quickly destroyed.

    using Clock = std::chrono::steady_clock;
    const auto waitDuration =
      std::chrono::duration_cast<Clock::duration>(mPeriod * (1.0 - mCpuUsage));

    setCurrentThreadName(mThreadName);

    sched_param param{};
    param.sched_priority = sched_get_priority_min(SCHED_OTHER);
    pthread_setschedparam(pthread_self(), SCHED_OTHER, &param);

    const auto isActive = [this] { return mIsActive.load(std::memory_order_acquire); };
    while (isActive())
    {
      const auto iterationStartTime = Clock::now();
      const auto waitEndTime = iterationStartTime + waitDuration;
      {
        std::unique_lock lock{mMutex};
        mConditionVariable.wait_until(lock, waitEndTime, [&] { return !isActive(); });
      }

      // To compensate for timing inaccuracy in the condition variable, we work until the
      // desired CPU usage is reached instead of working for a fixed duration. If the
      // target CPU usage is one we will stay in the loop until deactivated.
      const auto currentCpuUsage = [&, workStartTime = Clock::now()] {
        const auto currentTime = Clock::now();
        const auto workDuration = currentTime - workStartTime;
        const auto totalDuration = currentTime - iterationStartTime;
        return totalDuration.count() > 0
                 ? double(workDuration.count()) / double(totalDuration.count())
                 : 0.0;
      };
      while (currentCpuUsage() < mCpuUsage && isActive())
      {
        lowEnergyWork();
      }
    }
  }

  const std::string mThreadName;
  const Seconds mPeriod;
  const double mCpuUsage;
  std::mutex mMutex;
  std::condition_variable mConditionVariable;
  std::atomic<bool> mIsActive;
  std::thread mThread;
};


BusyThread::BusyThread(std::string threadName,
                       const Seconds period,
                       const double cpuUsage)
  : mpImpl{std::make_unique<BusyThreadImpl>(std::move(threadName), period, cpuUsage)}
{
}

BusyThread::BusyThread(BusyThread&&) noexcept = default;
BusyThread& BusyThread::operator=(BusyThread&&) noexcept = default;
BusyThread::~BusyThread() = default;


BusyThreads::BusyThreads() { setNumThreads(kDefaultNumBusyThreads); }

int BusyThreads::numThreads() const { return int(mThreads.size()); }
void BusyThreads::setNumThreads(const int numThreads)
{
  assertRelease(numThreads >= 0, "Invalid number of threads");

  if (numThreads != int(mThreads.size()))
  {
    rebuildThreads(numThreads);
  }
}

BusyThread::Seconds BusyThreads::period() const { return mPeriod; }
void BusyThreads::setPeriod(const Seconds period)
{
  if (period != mPeriod)
  {
    mPeriod = period;
    rebuildThreads(int(mThreads.size()));
  }
}

double BusyThreads::threadCpuUsage() const { return mThreadCpuUsage; }
void BusyThreads::setThreadCpuUsage(const double threadCpuUsage)
{
  if (threadCpuUsage != mThreadCpuUsage)
  {
    mThreadCpuUsage = threadCpuUsage;
    rebuildThreads(int(mThreads.size()));
  }
}

void BusyThreads::rebuildThreads(const int numThreads)
{
  assertRelease(numThreads >= 0, "Invalid number of threads");

  mThreads.clear();
  for (int threadIndex = 0; threadIndex < numThreads; ++threadIndex)
  {
    BusyThread thread{
      "Busy Thread " + std::to_string(threadIndex + 1), mPeriod, mThreadCpuUsage};
    mThreads.emplace_back(std::move(thread));
  }
}
