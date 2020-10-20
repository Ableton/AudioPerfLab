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

#include <chrono>
#include <tuple>

struct BusyThreadsConfig
{
  int numThreads{};
  std::chrono::duration<double> period{};
  double cpuUsage{};
};

inline bool operator==(const BusyThreadsConfig& lhs, const BusyThreadsConfig& rhs)
{
  return std::tie(lhs.numThreads, lhs.period, lhs.cpuUsage)
         == std::tie(rhs.numThreads, rhs.period, rhs.cpuUsage);
}

inline bool operator!=(const BusyThreadsConfig& lhs, const BusyThreadsConfig& rhs)
{
  return !(lhs == rhs);
}

struct AudioHostConfig
{
  int numProcessingThreads{};
  bool processInDriverThread{};
  bool isWorkIntervalOn{};
  double minimumLoad{};
};

inline bool operator==(const AudioHostConfig& lhs, const AudioHostConfig& rhs)
{
  return std::tie(lhs.numProcessingThreads, lhs.processInDriverThread,
                  lhs.isWorkIntervalOn, lhs.minimumLoad)
         == std::tie(rhs.numProcessingThreads, rhs.processInDriverThread,
                     rhs.isWorkIntervalOn, rhs.minimumLoad);
}

inline bool operator!=(const AudioHostConfig& lhs, const AudioHostConfig& rhs)
{
  return !(lhs == rhs);
}

struct PerformanceConfig
{
  BusyThreadsConfig busyThreads;
  AudioHostConfig audioHost;
};

inline bool operator==(const PerformanceConfig& lhs, const PerformanceConfig& rhs)
{
  return std::tie(lhs.busyThreads, lhs.audioHost)
         == std::tie(rhs.busyThreads, rhs.audioHost);
}

inline bool operator!=(const PerformanceConfig& lhs, const PerformanceConfig& rhs)
{
  return !(lhs == rhs);
}

constexpr auto kStandardPerformanceConfig = PerformanceConfig{
  BusyThreadsConfig{
    .numThreads = 0,
    // These settings are tuned to ramp up CPUs without exceeding the background CPU usage
    // limit. See the README for more information.
    .period = std::chrono::milliseconds{35},
    .cpuUsage = 0.5,
  },
  AudioHostConfig{
    .numProcessingThreads = 2,
    .processInDriverThread = true,
    .isWorkIntervalOn = true,
    .minimumLoad = 0.0,
  },
};

constexpr auto kOptimalPerformanceConfig = PerformanceConfig{
  BusyThreadsConfig{
    .numThreads = 1,
    .period = kStandardPerformanceConfig.busyThreads.period,
    .cpuUsage = kStandardPerformanceConfig.busyThreads.cpuUsage,
  },
  AudioHostConfig{
    .numProcessingThreads = 2,
    .processInDriverThread = false,
    .isWorkIntervalOn = false,
    .minimumLoad = kStandardPerformanceConfig.audioHost.minimumLoad,
  },
};

constexpr auto kCacheLineSize = 128;
constexpr auto kDefaultPreferredBufferSize = 128;
constexpr auto kRealtimeThreadQuantum = std::chrono::microseconds{500};
