// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/FftMapping.h"

#include <algorithm>
#include <cmath>

namespace esdr3 {

void mapFftToPixels(const float* db, int n, double fs,
                    double startRel, double endRel, int w,
                    float* outMax, float* outMin, float fill)
{
    if (n <= 0 || w <= 0 || fs <= 0 || endRel <= startRel) {
        for (int x = 0; x < w; ++x) {
            outMax[x] = fill;
            if (outMin) outMin[x] = fill;
        }
        return;
    }

    const double binHz = fs / n;
    const double hzPerPx = (endRel - startRel) / w;
    auto binOf = [&](double fRel) { return (fRel + fs / 2) / binHz; };

    for (int x = 0; x < w; ++x) {
        const double f0 = startRel + x * hzPerPx;
        const double f1 = f0 + hzPerPx;
        const double b0f = binOf(f0);
        const double b1f = binOf(f1);

        if (b1f <= 0 || b0f >= n) {
            outMax[x] = fill;
            if (outMin) outMin[x] = fill;
            continue;
        }

        int b0 = int(std::floor(b0f));
        int b1 = int(std::floor(b1f));

        if (b1 > b0) {
            b0 = std::max(b0, 0);
            b1 = std::min(b1, n);
            float mx = db[b0], mn = db[b0];
            for (int k = b0 + 1; k < b1; ++k) {
                mx = std::max(mx, db[k]);
                mn = std::min(mn, db[k]);
            }
            outMax[x] = mx;
            if (outMin) outMin[x] = mn;
        } else {
            const double bc = binOf(f0 + hzPerPx / 2) - 0.5;
            int i = int(std::floor(bc));
            double t = bc - i;
            if (i < 0) { i = 0; t = 0; }
            if (i >= n - 1) { i = n - 1; t = 0; }
            const float a = db[i];
            const float b = (i + 1 < n) ? db[i + 1] : a;
            const float v = float(a + (b - a) * t);
            outMax[x] = v;
            if (outMin) outMin[x] = v;
        }
    }
}

}
