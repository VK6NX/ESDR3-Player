// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDateTime>
#include <QMetaType>
#include <QString>
#include <cstdint>
#include <memory>
#include <vector>

namespace esdr3 {

enum class PlayState { Idle, Playing, Paused, Ended };

enum class Mode { CW = 0, USB, LSB, AM, FM };
enum class AgcMode { Off = 0, Fast, Slow };

struct DemodParams {
    Mode mode = Mode::CW;
    double tuneHz = 0;
    float bandwidthHz = 500;
    float cwPitchHz = 700;
    AgcMode agc = AgcMode::Slow;
    float volume = 0.5f;
    bool mute = false;

    bool operator==(const DemodParams& o) const
    {
        return mode == o.mode && tuneHz == o.tuneHz && bandwidthHz == o.bandwidthHz &&
               cwPitchHz == o.cwPitchHz && agc == o.agc && volume == o.volume && mute == o.mute;
    }
    bool operator!=(const DemodParams& o) const { return !(*this == o); }
};

struct EngineStatus {
    PlayState state = PlayState::Idle;
    uint64_t position = 0;
    uint64_t total = 0;
    double sampleRate = 0;
    double ddsHz = 0;
    QDateTime recStart;
    QString path;
    double speed = 1.0;
    bool loop = false;
};

struct PsdFrame {
    std::shared_ptr<const std::vector<float>> dB;
    int fftSize = 0;
    double sampleRate = 0;
    double centerHz = 0;
    uint64_t streamSample = 0;
};

}

Q_DECLARE_METATYPE(esdr3::EngineStatus)
Q_DECLARE_METATYPE(esdr3::PsdFrame)
Q_DECLARE_METATYPE(esdr3::DemodParams)
