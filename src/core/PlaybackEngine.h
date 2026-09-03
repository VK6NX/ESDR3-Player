// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/SpscRing.h"
#include "core/TailBuffer.h"
#include "core/Types.h"
#include "core/WavReader.h"
#include "dsp/Demodulator.h"

#include <QObject>
#include <QTimer>
#include <chrono>
#include <memory>
#include <vector>

namespace esdr3 {

class PlaybackEngine : public QObject {
    Q_OBJECT
public:
    PlaybackEngine(std::shared_ptr<IqTailBuffer> tail, std::shared_ptr<AudioRing> audio,
                   QObject* parent = nullptr);
    ~PlaybackEngine() override;

    static constexpr int kTickMs = 20;
    static constexpr size_t kBlock = 4096;

public slots:
    void open(const QString& path);
    void play();
    void pause();
    void togglePlay();
    void stop();
    void seek(double fraction);
    void setLoop(bool on);
    void setSpeed(double speed);
    void setDemod(const esdr3::DemodParams& params);
    void setAudioRate(double rate);
    void shutdown();

signals:
    void opened(esdr3::EngineStatus status);
    void status(esdr3::EngineStatus status);
    void error(const QString& message);
    void ended();

private:
    using Clock = std::chrono::steady_clock;

    void tick();
    void anchor();
    EngineStatus makeStatus() const;
    void emitStatus();

    void flushAudio();

    std::shared_ptr<IqTailBuffer> m_tail;
    std::shared_ptr<AudioRing> m_audio;
    QTimer* m_timer = nullptr;
    WavReader m_reader;
    Demodulator m_demod;
    DemodParams m_demodParams;
    double m_audioRate = 48000;
    std::vector<std::complex<float>> m_block;
    std::vector<float> m_audioBlock;
    PlayState m_state = PlayState::Idle;
    bool m_loop = false;
    double m_speed = 1.0;
    Clock::time_point m_anchorTime;
    uint64_t m_anchorSample = 0;
    int m_statusDivider = 0;
    double m_drift = 1.0;
};

}
