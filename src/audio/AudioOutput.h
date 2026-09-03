// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/SpscRing.h"

#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSink>
#include <QIODevice>
#include <QMediaDevices>
#include <QObject>
#include <atomic>
#include <memory>

namespace esdr3 {

class AudioOutput : public QObject {
    Q_OBJECT
public:
    explicit AudioOutput(std::shared_ptr<AudioRing> ring, QObject* parent = nullptr);
    ~AudioOutput() override;

    double sampleRate() const { return m_format.sampleRate(); }
    int underruns() const { return m_underruns.load(); }

public slots:
    void start();
    void setDevice(const QString& id);
    void stop();

signals:
    void started(double sampleRate, const QString& description);
    void error(const QString& message);

private:
    class RingDevice;
    bool open(const QAudioDevice& device);
    void onDevicesChanged();

    std::shared_ptr<AudioRing> m_ring;
    QAudioSink* m_sink = nullptr;
    RingDevice* m_device = nullptr;
    QMediaDevices* m_mediaDevices = nullptr;
    QAudioFormat m_format;
    QString m_deviceId;
    QByteArray m_openedId;
    std::atomic<int> m_underruns{0};
};

}
