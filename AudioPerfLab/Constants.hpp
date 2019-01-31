// Copyright: 2018, Ableton AG, Berlin. All rights reserved.

#pragma once

#include <chrono>
#include <initializer_list>

constexpr auto kDefaultNumWorkerThreads = 1;
constexpr auto kDefaultNumBusyThreads = 0;
constexpr auto kDefaultPreferredBufferSize = 128;
constexpr auto kDefaultNumSines = 60;
constexpr auto kAmpSmoothingDuration = std::chrono::milliseconds{100};

/* The number of partials taken at a time by worker threads.
 *
 * Partials are taken in chunks for a few reasons:
 *
 * - It simulates the workload of a real application, in which individual tasks are not
 *   sine waves but rather heavyweight items like synthesizers and audio effects.
 * - It forces worker threads to do a minimum amount of processing, provoking dropouts if
 *   workers are running slow.
 * - It avoids contention on the ParallelSineBank::mNumTakenPartials atomic.
 */
constexpr auto kNumPartialsPerProcessingChunk = 512;

// Play every note twice to increase load
constexpr auto kChordNoteNumbers = {41.0f, 41.0f, 44.0f, 44.0f, 48.0f, 48.0f};

constexpr auto kRealtimeThreadQuantum = std::chrono::microseconds{500};
constexpr auto kDriveMeasurementQueueSize = 1024;
constexpr auto kMaxNumFrames = 4096;
constexpr auto kCacheLineSize = 128;
