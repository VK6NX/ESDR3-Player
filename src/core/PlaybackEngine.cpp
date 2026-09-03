// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/PlaybackEngine.h"

#include <algorithm>
#include <cmath>

namespace esdr3 {

PlaybackEngine::PlaybackEngine(std::shared_ptr<IqTailBuffer> tail, std::shared_ptr<AudioRing> audio,
                               QObject* parent)
    : QObject(parent), m_tail(std::move(tail)), m_audio(std::move(audio)), m_block(kBlock),
      m_audioBlock(kBlock * 4 + 256)
{
    m_timer = new QTimer(this);
    m_timer->setTimerType(Qt::PreciseTimer);
    m_timer->setInterval(kTickMs);
    connect(m_timer, &QTimer::timeout, this, &PlaybackEngine::tick);
}

PlaybackEngine::~PlaybackEngine() = default;

EngineStatus PlaybackEngine::makeStatus() const
{
    EngineStatus s;
    s.state = m_state;
    s.position = m_reader.position();
    s.total = m_reader.totalSamples();
    s.sampleRate = m_reader.sampleRate();
    s.ddsHz = m_reader.ddsHz();
    s.recStart = m_reader.startTime();
    s.path = m_reader.path();
    s.speed = m_speed;
    s.loop = m_loop;
    return s;
}

void PlaybackEngine::emitStatus() { emit status(makeStatus()); }

void PlaybackEngine::open(const QString& path)
{
    m_timer->stop();
    m_state = PlayState::Idle;
    QString err;
    if (!m_reader.open(path, &err)) {
        emit error(QStringLiteral("%1: %2").arg(path, err));
        emitStatus();
        return;
    }
    m_tail->clear();
    flushAudio();
    m_demod.configure(m_reader.sampleRate(), m_audioRate);
    m_demod.setParams(m_demodParams);
    m_state = PlayState::Paused;
    emit opened(makeStatus());
    m_timer->start();
}

void PlaybackEngine::flushAudio()
{
    m_audio->requestFlush(size_t(m_audioRate * 0.15));
    m_demod.reset();
    m_drift = 1.0;
}

void PlaybackEngine::setDemod(const DemodParams& params)
{
    m_demodParams = params;
    m_demod.setParams(params);
}

void PlaybackEngine::setAudioRate(double rate)
{
    if (rate <= 0 || rate == m_audioRate) return;
    m_audioRate = rate;
    if (m_reader.isOpen()) {
        m_demod.configure(m_reader.sampleRate(), m_audioRate);
        m_demod.setParams(m_demodParams);
    }
}

void PlaybackEngine::anchor()
{
    m_anchorTime = Clock::now();
    m_anchorSample = m_reader.position();
}

void PlaybackEngine::play()
{
    if (!m_reader.isOpen()) return;
    if (m_state == PlayState::Ended) m_reader.seek(0);
    m_state = PlayState::Playing;
    anchor();
    emitStatus();
}

void PlaybackEngine::pause()
{
    if (m_state != PlayState::Playing) return;
    m_state = PlayState::Paused;
    flushAudio();
    emitStatus();
}

void PlaybackEngine::togglePlay()
{
    if (m_state == PlayState::Playing) pause();
    else play();
}

void PlaybackEngine::stop()
{
    if (!m_reader.isOpen()) return;
    m_state = PlayState::Paused;
    m_reader.seek(0);
    flushAudio();
    anchor();
    emitStatus();
}

void PlaybackEngine::seek(double fraction)
{
    if (!m_reader.isOpen()) return;
    fraction = std::clamp(fraction, 0.0, 1.0);
    m_reader.refreshLength();
    const uint64_t total = m_reader.totalSamples();
    m_reader.seek(uint64_t(std::llround(double(total) * fraction)));
    if (m_state == PlayState::Ended) m_state = PlayState::Paused;
    flushAudio();
    anchor();
    emitStatus();
}

void PlaybackEngine::setLoop(bool on)
{
    m_loop = on;
    emitStatus();
}

void PlaybackEngine::setSpeed(double speed)
{
    m_speed = std::clamp(speed, 0.25, 8.0);
    anchor();
    emitStatus();
}

void PlaybackEngine::shutdown()
{
    m_timer->stop();
    m_reader.close();
    m_state = PlayState::Idle;
}

void PlaybackEngine::tick()
{
    if (m_state == PlayState::Playing) {
        const double fs = m_reader.sampleRate();
        const auto now = Clock::now();

        if (m_speed == 1.0 && m_demod.isConfigured()) {
            const double fill = double(m_audio->available()) / double(m_audio->capacity());
            const double target = std::clamp(m_drift * (1.0 + 0.002 * (0.5 - fill) / 0.5), 0.997, 1.003);
            if (target != m_drift) {
                const double elapsedOld = std::chrono::duration<double>(now - m_anchorTime).count();
                m_anchorSample += uint64_t(std::llround(elapsedOld * fs * m_speed * m_drift));
                m_anchorTime = now;
                m_drift = target;
            }
        }

        const double elapsed = std::chrono::duration<double>(now - m_anchorTime).count();
        uint64_t due = m_anchorSample + uint64_t(std::llround(elapsed * fs * m_speed * m_drift));

        if (due > m_reader.position() + uint64_t(fs * 0.5)) {
            anchor();
            due = m_anchorSample + uint64_t(std::llround(fs * m_speed * kTickMs / 1000.0));
        }

        while (m_reader.position() < due && m_state == PlayState::Playing) {
            const size_t want = size_t(std::min<uint64_t>(kBlock, due - m_reader.position()));
            const size_t got = m_reader.read(m_block.data(), want);
            if (got > 0) {
                m_tail->push(m_block.data(), got, m_reader.position());
                if (m_speed == 1.0 && m_demod.isConfigured()) {
                    const size_t n = m_demod.process(m_block.data(), got, m_audioBlock.data(), m_audioBlock.size());
                    if (n > 0) m_audio->write(m_audioBlock.data(), n);
                }
                continue;
            }
            if (m_reader.refreshLength() && m_reader.position() < m_reader.totalSamples())
                continue;
            if (m_loop) {
                m_reader.seek(0);
                m_demod.reset();
                anchor();
                break;
            }
            m_state = PlayState::Ended;
            flushAudio();
            emit ended();
            emitStatus();
            break;
        }
    }

    if (++m_statusDivider >= 100 / kTickMs) {
        m_statusDivider = 0;
        if (m_state != PlayState::Idle) emitStatus();
    }
}

}
