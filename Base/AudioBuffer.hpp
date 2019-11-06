// Copyright: 2018, Ableton AG, Berlin. All rights reserved.

#pragma once

#include <array>
#include <vector>

using StereoAudioBuffer = std::array<std::vector<float>, 2>;
using StereoAudioBufferPtrs = std::array<float*, 2>;
