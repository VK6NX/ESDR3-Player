// SPDX-License-Identifier: GPL-3.0-or-later
#include "dsp/PsdComputer.h"

#include <liquid.h>

#include <cmath>

namespace esdr3 {

PsdComputer::PsdComputer(int fftSize)
    : m_n(fftSize), m_window(size_t(fftSize)), m_x(size_t(fftSize)), m_y(size_t(fftSize))
{
    const double N = double(fftSize - 1);
    double sum = 0;
    for (int i = 0; i < fftSize; ++i) {
        const double a = 2.0 * M_PI * i / N;
        const double w = 0.35875 - 0.48829 * std::cos(a) + 0.14128 * std::cos(2 * a) - 0.01168 * std::cos(3 * a);
        m_window[size_t(i)] = float(w);
        sum += w;
    }
    m_normDb = float(20.0 * std::log10(sum));

    m_plan = fft_create_plan(unsigned(fftSize),
                             reinterpret_cast<liquid_float_complex*>(m_x.data()),
                             reinterpret_cast<liquid_float_complex*>(m_y.data()),
                             LIQUID_FFT_FORWARD, 0);
}

PsdComputer::~PsdComputer()
{
    if (m_plan) fft_destroy_plan(static_cast<fftplan>(m_plan));
}

void PsdComputer::compute(const std::complex<float>* in, float* outDb)
{
    const size_t n = size_t(m_n);
    for (size_t i = 0; i < n; ++i)
        m_x[i] = in[i] * m_window[i];

    fft_execute(static_cast<fftplan>(m_plan));

    const size_t half = n / 2;
    for (size_t k = 0; k < n; ++k) {
        const std::complex<float>& v = m_y[(k + half) % n];
        const float p = v.real() * v.real() + v.imag() * v.imag();
        outDb[k] = 10.0f * std::log10(p + 1e-30f) - m_normDb;
    }
}

}
