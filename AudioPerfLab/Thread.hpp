// Copyright: 2018, Ableton AG, Berlin. All rights reserved.

#pragma once

#include <chrono>
#include <pthread.h>

#if defined(__SSE__)
#include <emmintrin.h>
#endif

uint64_t secondsToMachAbsoluteTime(std::chrono::duration<double> duration);
std::chrono::duration<double> machAbsoluteTimeToSeconds(uint64_t machTime);

struct TimeConstraintPolicy
{
  std::chrono::duration<double> period{};
  std::chrono::duration<double> quantum{};
  std::chrono::duration<double> constraint{};
};

void setThreadTimeConstraintPolicy(pthread_t thread,
                                   const TimeConstraintPolicy& timeConstraintPolicy);

void findAndJoinWorkInterval();
void leaveWorkInterval();

inline void hardwareDelay()
{
#if defined(__arm64__)
  // Enter a low power state until a wake-up event occurs. See the "Wait for Event
  // mechanism and Send event" section (D1.17.1) in the ARM Architecture Reference Manual
  // for ARMv8.
  //
  // In XNU an Event Stream (D10.2.3) produces a wake-up event every
  // ARM_BOARD_WFE_TIMEOUT_NS (currently 1us). In practice the instruction averages
  // 1.32us. XNU's implementation of machine_delay_until() also depends on this event
  // stream.
  __builtin_arm_wfe();
#elif defined(__arm__)
  __builtin_arm_yield();
#elif defined(__SSE__)
  _mm_pause();
#else
#error hardwareYield() not implemented on this architecture
#endif
}

template <typename Clock, typename Rep>
void hardwareDelayUntil(const std::chrono::time_point<Clock, Rep> until)
{
  while (Clock::now() < until)
  {
    hardwareDelay();
  }
}

// _os_cpu_number() from XNU with volatile inline assembly to prevent code from being
// moved out of a loop (see http://gcc.gnu.org/onlinedocs/gcc/Extended-Asm.html#Volatile).
inline unsigned int cpuNumber()
{
#if defined(__arm64__)
  uint64_t p;
  __asm__ volatile("mrs  %[p], TPIDRRO_EL0" : [p] "=&r"(p));
  return static_cast<unsigned int>(p & 0x7);
#elif defined(__arm__) && defined(_ARM_ARCH_6)
  uintptr_t p;
  __asm__ volatile("mrc  p15, 0, %[p], c13, c0, 3" : [p] "=&r"(p));
  return static_cast<unsigned int>(p & 0x3ul);
#elif defined(__x86_64__) || defined(__i386__)
  struct
  {
    uintptr_t p1, p2;
  } p;
  __asm__ volatile("sidt %[p]" : [p] "=&m"(p));
  return static_cast<unsigned int>(p.p1 & 0xfff);
#else
#error cpuNumber() not implemented on this architecture
#endif
}
