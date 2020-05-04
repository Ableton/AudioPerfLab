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

#pragma once

#include "Config.hpp"

#include <chrono>
#include <memory>
#include <string>
#include <vector>

class BusyThreadImpl;

/*! A low-priority thread that prevents CPU throttling by constantly performing low-energy
 * work.
 */
class BusyThread
{
public:
  using Seconds = std::chrono::duration<double>;

  explicit BusyThread(std::string threadName);

  BusyThread(BusyThread&&) noexcept;
  BusyThread& operator=(BusyThread&&) noexcept;

  BusyThread(const BusyThread&) = delete;
  BusyThread& operator=(const BusyThread&) = delete;

  ~BusyThread();

  //! Start performing busy work. A busy thread is stopped by default.
  void start();

  //! Stop performing busy work.
  void stop();

  //! The duration of one busy thread iteration.
  Seconds period() const;
  void setPeriod(Seconds period);

  //! The percentage of a busy thread iteration spent performing low-energy work rather
  //! than blocking.
  double threadCpuUsage() const;
  void setThreadCpuUsage(double threadCpuUsage);

private:
  // Use a Pimpl so that the class is movable
  std::unique_ptr<BusyThreadImpl> mpImpl;
};

class BusyThreads
{
public:
  using Seconds = BusyThread::Seconds;

  BusyThreads();

  int numThreads() const;
  void setNumThreads(int numThreads);

  //! The duration of one busy thread iteration.
  Seconds period() const;
  void setPeriod(Seconds period);

  //! The percentage of a busy thread iteration spent performing low-energy work rather
  //! than blocking.
  double threadCpuUsage() const;
  void setThreadCpuUsage(double threadCpuUsage);

private:
  std::vector<BusyThread> mThreads;
  Seconds mPeriod = kDefaultBusyThreadPeriod;
  double mThreadCpuUsage = kDefaultBusyThreadCpuUsage;
};
