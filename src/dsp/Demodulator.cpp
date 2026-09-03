// SPDX-License-Identifier: GPL-3.0-or-later
#include "dsp/Demodulator.h"

#include <liquid.h>

#include <algorithm>
#include <cmath>

namespace esdr3 {

namespace {

constexpr float kStopbandDb = 60.0f;
constexpr float kResampStopDb = 60.0f;
constexpr float kFmDeviationHz = 5000.0f;
constexpr float kAgcOutputLevel = 0.25f;
constexpr float kManualGainOff = 2000.0f;

inline liquid_float_complex* lc(std::complex<float>* p) { return reinterpret_cast<liquid_float_complex*>(p); }

}

Demodulator::Demodulator() = default;

Demodulator::~Demodulator()
{
    destroyFilter();
    destroyRateObjects();
    if (m_agc) agc_crcf_destroy(static_cast<agc_crcf>(m_agc));
    if (m_fm) freqdem_destroy(static_cast<freqdem>(m_fm));
    if (m_dcBlock) iirfilt_rrrf_destroy(static_cast<iirfilt_rrrf>(m_dcBlock));
}

void Demodulator::destroyRateObjects()
{
    if (m_nco) { nco_crcf_destroy(static_cast<nco_crcf>(m_nco)); m_nco = nullptr; }
    if (m_ifResamp) { msresamp_crcf_destroy(static_cast<msresamp_crcf>(m_ifResamp)); m_ifResamp = nullptr; }
    if (m_audioResamp) { msresamp_rrrf_destroy(static_cast<msresamp_rrrf>(m_audioResamp)); m_audioResamp = nullptr; }
}

void Demodulator::destroyFilter()
{
    if (m_bpf) { firfilt_cccf_destroy(static_cast<firfilt_cccf>(m_bpf)); m_bpf = nullptr; }
}

void Demodulator::configure(double inputRate, double audioRate)
{
    destroyRateObjects();
    m_inputRate = inputRate;
    m_audioRate = audioRate;
    if (inputRate <= 0 || audioRate <= 0) return;

    m_nco = nco_crcf_create(LIQUID_VCO);
    m_ifResamp = msresamp_crcf_create(float(kIfRate / inputRate), kResampStopDb);
    m_audioResamp = msresamp_rrrf_create(float(audioRate / kIfRate), kResampStopDb);

    if (!m_fm) m_fm = freqdem_create(kFmDeviationHz / float(kIfRate));
    if (!m_dcBlock) m_dcBlock = iirfilt_rrrf_create_dc_blocker(0.002f);
    m_deemphAlpha = 1.0f - std::exp(-2.0f * float(M_PI) * 2100.0f / float(kIfRate));

    if (!m_agc) rebuildAgc();
    rebuildFilter();
    setParams(m_params);
    reset();
}

void Demodulator::filterEdges(const DemodParams& p, float& lowHz, float& highHz)
{
    const float bw = std::max(50.0f, p.bandwidthHz);
    switch (p.mode) {
    case Mode::CW:
        lowHz = -bw / 2; highHz = bw / 2; break;
    case Mode::USB:
        lowHz = 200.0f; highHz = 200.0f + bw; break;
    case Mode::LSB:
        lowHz = -(200.0f + bw); highHz = -200.0f; break;
    case Mode::AM:
    case Mode::FM:
        lowHz = -bw / 2; highHz = bw / 2; break;
    }
}

void Demodulator::rebuildFilter()
{
    destroyFilter();
    if (!isConfigured()) return;

    float low = 0.0f, high = 0.0f;
    filterEdges(m_params, low, high);
    if (m_params.mode == Mode::CW) { low += m_params.cwPitchHz; high += m_params.cwPitchHz; }

    const float half = (high - low) / 2;
    const float center = (high + low) / 2;
    const float fc = std::clamp(half / float(kIfRate), 0.0005f, 0.49f);
    const float transition = std::max(50.0f, (high - low) * 0.2f) / float(kIfRate);
    unsigned len = estimate_req_filter_len(transition, kStopbandDb);
    len = std::clamp<unsigned>(len | 1u, 31u, 2047u);

    std::vector<float> h(len);
    liquid_firdes_kaiser(len, fc, kStopbandDb, 0.0f, h.data());

    std::vector<std::complex<float>> hc(len);
    const float w = 2.0f * float(M_PI) * center / float(kIfRate);
    for (unsigned i = 0; i < len; ++i)
        hc[i] = h[i] * std::complex<float>(std::cos(w * i), std::sin(w * i));

    m_bpf = firfilt_cccf_create(lc(hc.data()), len);
}

void Demodulator::rebuildAgc()
{
    if (m_agc) agc_crcf_destroy(static_cast<agc_crcf>(m_agc));
    auto q = agc_crcf_create();
    agc_crcf_set_bandwidth(q, m_params.agc == AgcMode::Fast ? 0.02f : 0.002f);
    m_agc = q;
}

void Demodulator::setParams(const DemodParams& p)
{
    const bool filterChanged = p.mode != m_params.mode || p.bandwidthHz != m_params.bandwidthHz ||
                               (p.mode == Mode::CW && p.cwPitchHz != m_params.cwPitchHz);
    const bool agcChanged = p.agc != m_params.agc;
    m_params = p;
    if (!isConfigured()) return;

    const double shift = p.mode == Mode::CW ? p.tuneHz - p.cwPitchHz : p.tuneHz;
    nco_crcf_set_frequency(static_cast<nco_crcf>(m_nco), float(2.0 * M_PI * shift / m_inputRate));

    if (filterChanged) rebuildFilter();
    if (agcChanged) rebuildAgc();
    m_manualGain = kManualGainOff;
}

void Demodulator::reset()
{
    if (m_ifResamp) msresamp_crcf_reset(static_cast<msresamp_crcf>(m_ifResamp));
    if (m_audioResamp) msresamp_rrrf_reset(static_cast<msresamp_rrrf>(m_audioResamp));
    if (m_bpf) firfilt_cccf_reset(static_cast<firfilt_cccf>(m_bpf));
    if (m_agc) agc_crcf_reset(static_cast<agc_crcf>(m_agc));
    if (m_fm) freqdem_reset(static_cast<freqdem>(m_fm));
    if (m_dcBlock) iirfilt_rrrf_reset(static_cast<iirfilt_rrrf>(m_dcBlock));
    m_deemph = 0;
}

size_t Demodulator::process(const std::complex<float>* in, size_t n, float* out, size_t outCap)
{
    if (!isConfigured() || n == 0 || !m_bpf) return 0;

    m_mixed.resize(n);
    nco_crcf_mix_block_down(static_cast<nco_crcf>(m_nco),
                            lc(const_cast<std::complex<float>*>(in)), lc(m_mixed.data()), unsigned(n));

    const size_t ifCap = size_t(std::ceil(n * kIfRate / m_inputRate)) + 64;
    m_if.resize(ifCap);
    unsigned nIf = 0;
    msresamp_crcf_execute(static_cast<msresamp_crcf>(m_ifResamp), lc(m_mixed.data()), unsigned(n),
                          lc(m_if.data()), &nIf);
    if (nIf == 0) return 0;

    m_filt.resize(nIf);
    firfilt_cccf_execute_block(static_cast<firfilt_cccf>(m_bpf), lc(m_if.data()), nIf, lc(m_filt.data()));

    if (m_params.agc != AgcMode::Off) {
        agc_crcf_execute_block(static_cast<agc_crcf>(m_agc), lc(m_filt.data()), nIf, lc(m_filt.data()));
        for (unsigned i = 0; i < nIf; ++i) m_filt[i] *= kAgcOutputLevel;
    } else {
        for (unsigned i = 0; i < nIf; ++i) m_filt[i] *= m_manualGain;
    }

    m_audioIf.resize(nIf);
    switch (m_params.mode) {
    case Mode::CW:
    case Mode::USB:
    case Mode::LSB:
        for (unsigned i = 0; i < nIf; ++i) m_audioIf[i] = m_filt[i].real();
        break;
    case Mode::AM:
        for (unsigned i = 0; i < nIf; ++i) {
            float y;
            iirfilt_rrrf_execute(static_cast<iirfilt_rrrf>(m_dcBlock), std::abs(m_filt[i]), &y);
            m_audioIf[i] = y;
        }
        break;
    case Mode::FM:
        freqdem_demodulate_block(static_cast<freqdem>(m_fm), lc(m_filt.data()), nIf, m_audioIf.data());
        for (unsigned i = 0; i < nIf; ++i) {
            m_deemph += m_deemphAlpha * (m_audioIf[i] - m_deemph);
            m_audioIf[i] = m_deemph;
        }
        break;
    }

    unsigned nOut = 0;
    if (outCap < size_t(nIf * m_audioRate / kIfRate) + 8) return 0;
    msresamp_rrrf_execute(static_cast<msresamp_rrrf>(m_audioResamp), m_audioIf.data(), nIf, out, &nOut);

    const float vol = m_params.mute ? 0.0f : m_params.volume * m_params.volume;
    for (unsigned i = 0; i < nOut; ++i)
        out[i] = std::tanh(out[i] * vol);
    return nOut;
}

}
