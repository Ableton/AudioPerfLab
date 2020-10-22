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

#include "Warnings.hpp"

#include <os/workgroup.h>
#include <variant>

/*! A safe C++ wrapper around the Audio Workgroup API.
 *
 * See https://developer.apple.com/documentation/audiotoolbox/workgroup_management
 * and https://developer.apple.com/videos/play/wwdc2020/10224/
 */
class AudioWorkgroup
{
public:
  API_AVAILABLE(ios(14.0))
  explicit AudioWorkgroup(os_workgroup_t pWorkgroup);

  AudioWorkgroup(const AudioWorkgroup&);
  AudioWorkgroup& operator=(const AudioWorkgroup&);
  ~AudioWorkgroup();

  /*! The system's recommendation for the maximum number of threads that should contribute
   * to the workload.
   *
   * iOS 14, for example, recommends a thread per performance core for the audio I/O
   * unit's workgroup.
   *
   * @see os_workgroup_max_parallel_threads
   */
  int maxNumParallelThreads() const;

  class ScopedMembership;

  /*! Join the current thread to the workgroup. */
  ScopedMembership join();

private:
  DISABLE_AVAILABILITY_WARNINGS
  os_workgroup_t mpWorkgroup;
  RESTORE_WARNINGS
};

/*! A handle representing a thread's workgroup membership.
 *
 * The current thread is removed from the workgroup upon destruction.
 */
class AudioWorkgroup::ScopedMembership
{
public:
  ScopedMembership(ScopedMembership&&);
  ScopedMembership(const ScopedMembership&) = delete;
  ScopedMembership& operator=(ScopedMembership&&);
  ScopedMembership& operator=(const ScopedMembership&) = delete;
  ~ScopedMembership();

private:
  API_AVAILABLE(ios(14.0))
  explicit ScopedMembership(os_workgroup_t workgroup);

  DISABLE_AVAILABILITY_WARNINGS
  os_workgroup_t mpWorkgroup;
  os_workgroup_join_token_s mJoinToken;
  RESTORE_WARNINGS

  friend class AudioWorkgroup;
};

/*! A wrapper around a private work interval API that can be used prior to iOS 14.
 *
 * Note: private APIs may stop working at any time and their use is forbidden in the App
 * Store. This class should not be used in production apps.
 */
class LegacyAudioWorkgroup
{
public:
  /*! The system's recommendation for the maximum number of threads that should contribute
   * to the workload.
   *
   * @see pthread_time_constraint_max_parallelism
   */
  int maxNumParallelThreads() const;

  class ScopedMembership;

  /*! Join the current thread to the workgroup. */
  ScopedMembership join();
};

/*! A handle representing a thread's workgroup membership.
 *
 * The current thread is removed from the workgroup upon destruction.
 */
class LegacyAudioWorkgroup::ScopedMembership
{
public:
  ScopedMembership(ScopedMembership&&);
  ScopedMembership(const ScopedMembership&) = delete;
  ScopedMembership& operator=(ScopedMembership&&);
  ScopedMembership& operator=(const ScopedMembership&) = delete;
  ~ScopedMembership();

private:
  ScopedMembership();
  bool mIsActive;

  friend class LegacyAudioWorkgroup;
};

/*! A wrapper around either AudioWorkgroup or LegacyAudioWorkgroup.
 *
 * A client should check the running OS version using the `available` keyword and pass
 * either an AudioWorkgroup (>= iOS 14) or a LegacyAudioWorkgroup (< iOS 14) to the
 * constructor.
 */
class SomeAudioWorkgroup
{
public:
  using WorkgroupVariant = std::variant<AudioWorkgroup, LegacyAudioWorkgroup>;
  using ScopedMembership = std::variant<AudioWorkgroup::ScopedMembership,
                                        LegacyAudioWorkgroup::ScopedMembership>;

  explicit SomeAudioWorkgroup(const WorkgroupVariant& audioWorkgroup);

  /*! The system's recommendation for the maximum number of threads that should contribute
   * to the workload.
   */
  int maxNumParallelThreads() const;

  /*! Join the current thread to the workgroup. */
  ScopedMembership join();

private:
  WorkgroupVariant mWorkgroup;
};
