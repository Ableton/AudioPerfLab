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

#include <cstdint>
#include <mach/mach_types.h>

/*! A system-provided counting semaphore.
 *
 * A counting semaphore is synchronization primitive that is conceptually an integer that
 * is never less than 0.  There are two main operations: post (increment) which increases
 * the count, and wait (decrement) which decreases the count.  If a thread attempts to
 * wait and the count is zero, it will block until another thread posts.
 *
 * Semaphores are useful for producer/consumer relationships where threads need to be
 * woken up to consume some number of resources.  Aside from being relatively fast, they
 * are particularly useful compared to locks and conditions because the "signal" is
 * persistent: unlike with condition variables, a "signal" (post) can not be missed, since
 * a thread does not need to be waiting in order to see the changed value.
 *
 * For example, if a thread P is writing items to a queue, and a thread C is reading
 * elements from that queue, P can post to a semaphore every time an item is written.  C
 * can then wait on that semaphore before reading an event from the queue.  If P writes 8
 * items, then it will increment the semaphore 8 times, so C will "wake up" 8 times, even
 * if C is not waiting on the semaphore at the time P posts to it.
 */
class Semaphore
{
public:
  enum class Status
  {
    success,
    error,
  };

  explicit Semaphore(uint32_t initial);
  ~Semaphore();

  Semaphore(const Semaphore&) = delete;
  Semaphore& operator=(const Semaphore&) = delete;

  /*! Post to (increment) the semaphore, and wake up one waiter if necessary.
   *
   * If any threads are waiting, exactly one will be woken up.  Otherwise, one thread will
   * not block in `wait()` in the future.
   */
  Status post();

  /*! Wait on (decrement) the semaphore, blocking if necessary.
   *
   * @return Success, or error, in which case the semaphore may no longer be used.
   */
  Status wait();

private:
  semaphore_t mSemaphore;
};
