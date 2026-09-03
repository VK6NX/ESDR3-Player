// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <complex>
#include <cstddef>
#include <vector>

namespace esdr3 {

class PsdComputer {
public:
    explicit PsdComputer(int fftSize);
    ~PsdComputer();
    PsdComputer(const PsdComputer&) = delete;
    PsdComputer& operator=(const PsdComputer&) = delete;

    int size() const { return m_n; }

    void compute(const std::complex<float>* in, float* outDb);

private:
    int m_n;
    std::vector<float> m_window;
    float m_normDb = 0;
    std::vector<std::complex<float>> m_x;
    std::vector<std::complex<float>> m_y;
    void* m_plan = nullptr;
};

}
