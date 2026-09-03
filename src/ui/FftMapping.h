// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstddef>

namespace esdr3 {

void mapFftToPixels(const float* db, int n, double fs,
                    double startRel, double endRel, int w,
                    float* outMax, float* outMin, float fill);

}
