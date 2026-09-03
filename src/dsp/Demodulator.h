// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/Types.h"

#include <complex>
#include <cstddef>
#include <vector>

namespace esdr3 {

class Demodulator {
public:
    static constexpr double kIfRate = 24000.0;

    Demodulator();
    ~Demodulator();
    Demodulator(const Demodulator&) = delete;
    Demodulator& operator=(const Demodulator&) = delete;

    void configure(double inputRate, double audioRate);
    bool isConfigured() const { return m_inputRate > 0 && m_audioRate > 0; }
    double audioRate() const { return m_audioRate; }

    void setParams(const DemodParams& p);
    const DemodParams& params() const { return m_params; }

    size_t process(const std::complex<float>* in, size_t n, float* out, size_t outCap);

    void reset();

    static void filterEdges(const DemodParams& p, float& lowHz, float& highHz);

private:
    void rebuildFilter();
    void rebuildAgc();
    void destroyRateObjects();
    void destroyFilter();

    DemodParams m_params;
    double m_inputRate = 0;
    double m_audioRate = 0;

    void* m_nco = nullptr;
    void* m_ifResamp = nullptr;
    void* m_bpf = nullptr;
    void* m_agc = nullptr;
    void* m_fm = nullptr;
    void* m_dcBlock = nullptr;
    void* m_audioResamp = nullptr;

    float m_deemph = 0;
    float m_deemphAlpha = 0;
    float m_manualGain = 1.0f;

    std::vector<std::complex<float>> m_mixed;
    std::vector<std::complex<float>> m_if;
    std::vector<std::complex<float>> m_filt;
    std::vector<float> m_audioIf;
};

}
