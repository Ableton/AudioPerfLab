// Copyright: 2018, Ableton AG, Berlin. All rights reserved.

#include "Base/Semaphore.hpp"

#include <cstdint>
#include <mach/mach_init.h>
#include <mach/semaphore.h>
#include <mach/sync_policy.h>
#include <mach/task.h>
#include <stdexcept>

#if defined(__has_feature) && __has_feature(thread_sanitizer)
#define ANNOTATE_HAPPENS_BEFORE(addr)                                                    \
  AnnotateHappensBefore(__FILE__, __LINE__, (void*)(addr))
#define ANNOTATE_HAPPENS_AFTER(addr)                                                     \
  AnnotateHappensAfter(__FILE__, __LINE__, (void*)(addr))
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
