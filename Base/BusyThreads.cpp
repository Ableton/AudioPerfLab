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
#include <optional>
#include <pthread.h>
#include <pthread/sched.h>
#include <thread>

class BusyThreadImpl
{
public:
  using Seconds = BusyThread::Seconds;

  explicit BusyThreadImpl(std::string threadName)
    : mThreadName{std::move(threadName)}
  {
  }

  ~BusyThreadImpl() { stop(); }

  void start()
  {
    assertRelease(mIsActive == mThread.joinable(), "Invalid busy thread state");

    if (!mIsActive)
    {
      mIsActive = true;
      mThread = std::thread{&BusyThreadImpl::busyThread, this};
    }
  }

  void stop()
  {
    assertRelease(mIsActive == mThread.joinable(), "Invalid busy thread state");

    if (mIsActive)
    {
      {
        std::unique_lock lock{mMutex};
        mIsActive = false;
        mConditionVariable.notify_all();
      }

      mThread.join();
    }
  }

  BusyThread::Seconds period() const { return mPeriod; }
  void setPeriod(const Seconds period)
  {
    assertRelease(period > Seconds{0.0}, "Invalid busy thread period");

    if (period != mPeriod)
    {
      std::unique_lock lock{mMutex};
      mPeriod = period;
    }
  }

  double threadCpuUsage() const { return mThreadCpuUsage; }
  void setThreadCpuUsage(const double threadCpuUsage)
  {
    assertRelease(
      threadCpuUsage >= 0.0 && threadCpuUsage <= 1.0, "Invalid busy thread CPU usage");

    if (threadCpuUsage != mThreadCpuUsage)
    {
      std::unique_lock lock{mMutex};
      mThreadCpuUsage = threadCpuUsage;
    }
  }

private:
  void busyThread()
  {
    // A busy thread alternates between blocking on a condition variable and performing
    // low-energy work via a hardware delay instruction. Blocking avoids being terminated
    // when running in the background by violating the iOS CPU usage limit. The CPU usage
    // percentage needs to be set high enough to prevent CPU throttling and low enough to
    // avoid background termination.
    //
    // The use of mIsActive and the condition variable allows the thread to be quickly
    // destroyed, for example when BusyThreads::setNumThreads() is called.

    using Clock = std::chrono::steady_clock;
    using TimePoint = std::chrono::time_point<Clock, Seconds>;

    setCurrentThreadName(mThreadName);

    sched_param param{};
    param.sched_priority = sched_get_priority_min(SCHED_OTHER);
    pthread_setschedparam(pthread_self(), SCHED_OTHER, &param);

    const auto getDelayEndTimeAndBlock = [&] {
      const auto startTime = Clock::now();

      std::unique_lock lock{mMutex};

      const auto lowEnergyDelayDuration = mPeriod * mThreadCpuUsage;
      const auto blockDuration = mPeriod - lowEnergyDelayDuration;
      const auto blockEndTime = startTime + blockDuration;
      const auto delayEndTime = blockEndTime + lowEnergyDelayDuration;

      mConditionVariable.wait_until(lock, blockEndTime, [this] { return !mIsActive; });
      return mIsActive ? std::optional<TimePoint>{delayEndTime}
                       : std::optional<TimePoint>{};
    };

    while (const auto delayEndTime = getDelayEndTimeAndBlock())
    {
      while (Clock::now() < *delayEndTime && mIsActive)
      {
        lowEnergyWork();
      }
    }
  }

  const std::string mThreadName;
  std::thread mThread;
  std::condition_variable mConditionVariable;
  std::mutex mMutex;
  std::atomic<bool> mIsActive{false};
  Seconds mPeriod = kDefaultBusyThreadPeriod;
  double mThreadCpuUsage = kDefaultBusyThreadCpuUsage;
};


BusyThread::BusyThread(std::string threadName)
  : mpImpl{std::make_unique<BusyThreadImpl>(std::move(threadName))}
{
}

BusyThread::BusyThread(BusyThread&&) noexcept = default;
BusyThread& BusyThread::operator=(BusyThread&&) noexcept = default;
BusyThread::~BusyThread() = default;

void BusyThread::start() { mpImpl->start(); }
void BusyThread::stop() { mpImpl->stop(); }

BusyThread::Seconds BusyThread::period() const { return mpImpl->period(); }
void BusyThread::setPeriod(const Seconds period) { mpImpl->setPeriod(period); }

double BusyThread::threadCpuUsage() const { return mpImpl->threadCpuUsage(); }
void BusyThread::setThreadCpuUsage(const double threadCpuUsage)
{
  mpImpl->setThreadCpuUsage(threadCpuUsage);
}


BusyThreads::BusyThreads() { setNumThreads(kDefaultNumBusyThreads); }

int BusyThreads::numThreads() const { return int(mThreads.size()); }
void BusyThreads::setNumThreads(const int numThreads)
{
  assertRelease(numThreads >= 0, "Invalid number of threads");

  if (numThreads != int(mThreads.size()))
  {
    mThreads.clear();
    for (int threadIndex = 0; threadIndex < numThreads; ++threadIndex)
    {
      BusyThread thread{"Busy Thread " + std::to_string(threadIndex + 1)};
      thread.setPeriod(mPeriod);
      thread.setThreadCpuUsage(mThreadCpuUsage);
      thread.start();
      mThreads.emplace_back(std::move(thread));
    }
  }
}

BusyThread::Seconds BusyThreads::period() const { return mPeriod; }
void BusyThreads::setPeriod(const Seconds period)
{
  if (period != mPeriod)
  {
    for (auto& thread : mThreads)
    {
      thread.setPeriod(period);
    }
    mPeriod = period;
  }
}

double BusyThreads::threadCpuUsage() const { return mThreadCpuUsage; }
void BusyThreads::setThreadCpuUsage(const double threadCpuUsage)
{
  if (threadCpuUsage != mThreadCpuUsage)
  {
    for (auto& thread : mThreads)
    {
      thread.setThreadCpuUsage(threadCpuUsage);
    }
    mThreadCpuUsage = threadCpuUsage;
  }
}
