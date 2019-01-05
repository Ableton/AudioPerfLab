// Copyright: 2018, Ableton AG, Berlin. All rights reserved.

#pragma once

#include <chrono>
#include <initializer_list>

constexpr auto kDefaultNumWorkerThreads = 0;
constexpr auto kDefaultNumBusyThreads = 0;
constexpr auto kDefaultPreferredBufferSize = 1024;
constexpr auto kDefaultNumSines = 60;
constexpr auto kAmpSmoothingDuration = std::chrono::milliseconds{100};
constexpr auto kNumPartialsPerProcessingChunk = 32;

// Play every note twice to increase load
constexpr auto kChordNoteNumbers = {41.0f, 41.0f, 44.0f, 44.0f, 48.0f, 48.0f};

constexpr auto kRealtimeThreadQuantum = std::chrono::microseconds{500};
constexpr auto kDriveMeasurementQueueSize = 1024;
constexpr auto kMaxNumFrames = 4096;
constexpr auto kCacheLineSize = 128;
