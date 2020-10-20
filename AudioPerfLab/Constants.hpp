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
#include <initializer_list>

constexpr auto kDefaultNumSines = 18;
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
constexpr auto kNumPartialsPerProcessingChunk = 256;

constexpr auto kChordNoteNumbers = {53.0f, 56.0f, 60.0f};
constexpr auto kNumUnrandomizedPhases = 15;

constexpr auto kDriveMeasurementQueueSize = 1024;
constexpr auto kMaxNumFrames = 4096;
