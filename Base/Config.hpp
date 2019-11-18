// Copyright: 2019, Ableton AG, Berlin. All rights reserved.

#pragma once

#include <chrono>

constexpr auto kDefaultNumWorkerThreads = 1;
constexpr auto kDefaultNumBusyThreads = 0;

constexpr auto kCacheLineSize = 128;
constexpr auto kDefaultPreferredBufferSize = 128;
constexpr auto kRealtimeThreadQuantum = std::chrono::microseconds{500};
