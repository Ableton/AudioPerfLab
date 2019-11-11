// Copyright: 2019, Ableton AG, Berlin. All rights reserved.

#pragma once

#include <cstdlib>
#include <iostream>

inline void assertRelease(const bool condition, const char* pMessage)
{
  if (!condition)
  {
    std::cerr << pMessage << '\n';
    std::abort();
  }
}
