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

#include "Base/Semaphore.hpp"

#include <cstdint>
#include <mach/mach_init.h>
#include <mach/semaphore.h>
#include <mach/sync_policy.h>
#include <mach/task.h>
#include <stdexcept>

#if defined(__has_feature) && __has_feature(thread_sanitizer)
#define ANNOTATE_HAPPENS_BEFORE(addr)                                                    \
  AnnotateHappensBefore(__FILE__, __LINE__, static_cast<void*>(addr))
#define ANNOTATE_HAPPENS_AFTER(addr)                                                     \
  AnnotateHappensAfter(__FILE__, __LINE__, static_cast<void*>(addr))
extern "C" void AnnotateHappensBefore(const char* f, int l, void* addr);
extern "C" void AnnotateHappensAfter(const char* f, int l, void* addr);
#else
#define ANNOTATE_HAPPENS_BEFORE(addr)
#define ANNOTATE_HAPPENS_AFTER(addr)
#endif

Semaphore::Semaphore(const uint32_t initial)
{
  if (semaphore_create(mach_task_self(), &mSemaphore, SYNC_POLICY_FIFO, int(initial)))
  {
    throw std::runtime_error("Error creating semaphore");
  }
}

Semaphore::~Semaphore() { semaphore_destroy(mach_task_self(), mSemaphore); }

Semaphore::Status Semaphore::post()
{
  ANNOTATE_HAPPENS_BEFORE(&mSemaphore);
  return !semaphore_signal(mSemaphore) ? Status::success : Status::error;
}

Semaphore::Status Semaphore::wait()
{
  kern_return_t result;
  do
  {
    result = semaphore_wait(mSemaphore);
  } while (result == KERN_ABORTED);

#if defined(__has_feature) && __has_feature(thread_sanitizer)
  if (result == KERN_SUCCESS)
  {
    ANNOTATE_HAPPENS_AFTER(&mSemaphore);
  }
#endif

  return result == KERN_SUCCESS ? Status::success : Status::error;
}
