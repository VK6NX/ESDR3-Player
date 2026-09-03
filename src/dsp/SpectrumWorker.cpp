// SPDX-License-Identifier: GPL-3.0-or-later
#include "dsp/SpectrumWorker.h"

#include <algorithm>

namespace esdr3 {

SpectrumWorker::SpectrumWorker(std::shared_ptr<IqTailBuffer> tail, QObject* parent)
    : QObject(parent), m_tail(std::move(tail))
{
    m_timer = new QTimer(this);
    m_timer->setTimerType(Qt::PreciseTimer);
    m_timer->setInterval(40);
    connect(m_timer, &QTimer::timeout, this, &SpectrumWorker::tick);
}

SpectrumWorker::~SpectrumWorker() = default;

void SpectrumWorker::start()
{
    if (!m_psd) rebuild();
    m_timer->start();
}

void SpectrumWorker::stop() { m_timer->stop(); }

void SpectrumWorker::setFftSize(int n)
{
    n = std::clamp(n, 1024, 65536);
    int p = 1024;
    while (p < n) p <<= 1;
    if (p == m_fftSize && m_psd) return;
    m_fftSize = p;
    rebuild();
}

void SpectrumWorker::setAveraging(float a) { m_averaging = std::clamp(a, 0.0f, 0.95f); }

void SpectrumWorker::setFrameRate(int fps)
{
    fps = std::clamp(fps, 5, 60);
    m_timer->setInterval(1000 / fps);
}

void SpectrumWorker::setSampleRate(double fs) { m_sampleRate = fs; }
void SpectrumWorker::setCenterHz(double hz) { m_centerHz = hz; }

void SpectrumWorker::reset()
{
    m_avgValid = false;
    m_lastStreamPos = ~0ull;
}

void SpectrumWorker::rebuild()
{
    m_psd = std::make_unique<PsdComputer>(m_fftSize);
    m_in.assign(size_t(m_fftSize), {});
    m_db.assign(size_t(m_fftSize), 0.0f);
    m_avg.assign(size_t(m_fftSize), 0.0f);
    m_avgValid = false;
    m_lastStreamPos = ~0ull;
}

void SpectrumWorker::tick()
{
    if (!m_psd || m_sampleRate <= 0) return;

    uint64_t streamPos = 0;
    if (!m_tail->copyLast(m_in.data(), size_t(m_fftSize), &streamPos)) return;
    if (streamPos == m_lastStreamPos) return;
    m_lastStreamPos = streamPos;

    m_psd->compute(m_in.data(), m_db.data());

    const size_t n = size_t(m_fftSize);
    if (!m_avgValid || m_averaging <= 0.0f) {
        std::copy(m_db.begin(), m_db.end(), m_avg.begin());
        m_avgValid = true;
    } else {
        const float a = m_averaging, b = 1.0f - a;
        for (size_t i = 0; i < n; ++i)
            m_avg[i] = a * m_avg[i] + b * m_db[i];
    }

    PsdFrame frame;
    frame.dB = std::make_shared<const std::vector<float>>(m_avg);
    frame.fftSize = m_fftSize;
    frame.sampleRate = m_sampleRate;
    frame.centerHz = m_centerHz;
    frame.streamSample = streamPos;
    emit psdReady(frame);
}

}
