// SPDX-License-Identifier: GPL-3.0-or-later
#include "audio/AudioOutput.h"

#include <QLoggingCategory>
#include <QMediaDevices>
#include <algorithm>
#include <cstring>
#include <vector>

Q_LOGGING_CATEGORY(lcAudio, "esdr3.audio", QtInfoMsg)

namespace esdr3 {

class AudioOutput::RingDevice : public QIODevice {
public:
    RingDevice(std::shared_ptr<AudioRing> ring, QAudioFormat format, std::atomic<int>& underruns)
        : m_ring(std::move(ring)), m_format(format), m_underruns(underruns)
    {
        open(QIODevice::ReadOnly | QIODevice::Unbuffered);
    }

    qint64 readData(char* data, qint64 maxlen) override
    {
        const int channels = m_format.channelCount();
        const int bytesPerSample = m_format.bytesPerSample();
        const qint64 frameBytes = qint64(channels) * bytesPerSample;
        if (frameBytes <= 0) return 0;
        const size_t frames = size_t(maxlen / frameBytes);
        if (frames == 0) return 0;

        m_mono.resize(frames);
        const size_t got = m_ring->read(m_mono.data(), frames);
        if (got < frames) {
            std::fill(m_mono.begin() + qsizetype(got), m_mono.end(), 0.0f);
            if (got == 0 && m_hadData) m_underruns.fetch_add(1);
        } else {
            m_hadData = true;
        }

        char* p = data;
        for (size_t i = 0; i < frames; ++i) {
            const float v = m_mono[i];
            for (int c = 0; c < channels; ++c) {
                switch (m_format.sampleFormat()) {
                case QAudioFormat::Float: {
                    std::memcpy(p, &v, sizeof v);
                    break;
                }
                case QAudioFormat::Int16: {
                    const int16_t s = int16_t(std::clamp(v, -1.0f, 1.0f) * 32767.0f);
                    std::memcpy(p, &s, sizeof s);
                    break;
                }
                case QAudioFormat::Int32: {
                    const int32_t s = int32_t(std::clamp(v, -1.0f, 1.0f) * 2147483647.0f);
                    std::memcpy(p, &s, sizeof s);
                    break;
                }
                default:
                    std::memset(p, 0, size_t(bytesPerSample));
                    break;
                }
                p += bytesPerSample;
            }
        }
        return qint64(frames) * frameBytes;
    }

    qint64 writeData(const char*, qint64) override { return -1; }
    qint64 bytesAvailable() const override { return 1 << 20; }
    bool isSequential() const override { return true; }

private:
    std::shared_ptr<AudioRing> m_ring;
    QAudioFormat m_format;
    std::atomic<int>& m_underruns;
    std::vector<float> m_mono;
    bool m_hadData = false;
};

AudioOutput::AudioOutput(std::shared_ptr<AudioRing> ring, QObject* parent)
    : QObject(parent), m_ring(std::move(ring))
{
}

AudioOutput::~AudioOutput() { stop(); }

void AudioOutput::start()
{
    if (!m_mediaDevices) {
        m_mediaDevices = new QMediaDevices(this);
        connect(m_mediaDevices, &QMediaDevices::audioOutputsChanged, this, &AudioOutput::onDevicesChanged);
    }
    if (!m_sink) setDevice(m_deviceId);
}

void AudioOutput::setDevice(const QString& id)
{
    m_deviceId = id;
    QAudioDevice device = QMediaDevices::defaultAudioOutput();
    if (!id.isEmpty()) {
        for (const auto& d : QMediaDevices::audioOutputs())
            if (QString::fromLatin1(d.id()) == id) { device = d; break; }
    }
    if (device.isNull()) {
        emit error(QStringLiteral("No audio output device"));
        return;
    }
    open(device);
}

void AudioOutput::onDevicesChanged()
{
    QAudioDevice want = QMediaDevices::defaultAudioOutput();
    if (!m_deviceId.isEmpty()) {
        bool found = false;
        for (const auto& d : QMediaDevices::audioOutputs())
            if (QString::fromLatin1(d.id()) == m_deviceId) { want = d; found = true; break; }
        if (found && want.id() == m_openedId) return;
    } else if (want.id() == m_openedId) {
        return;
    }
    qCInfo(lcAudio) << "audio devices changed, reopening";
    if (!want.isNull()) open(want);
}

bool AudioOutput::open(const QAudioDevice& device)
{
    stop();

    QAudioFormat f = device.preferredFormat();
    f.setChannelCount(1);
    f.setSampleFormat(QAudioFormat::Float);
    if (!device.isFormatSupported(f)) {
        f = device.preferredFormat();
        f.setSampleFormat(QAudioFormat::Float);
        if (!device.isFormatSupported(f)) {
            f = device.preferredFormat();
            f.setSampleFormat(QAudioFormat::Int16);
            if (!device.isFormatSupported(f)) f = device.preferredFormat();
        }
    }
    m_format = f;
    m_openedId = device.id();

    m_device = new RingDevice(m_ring, f, m_underruns);
    m_sink = new QAudioSink(device, f, this);
    m_sink->setBufferSize(f.bytesForDuration(100000));
    m_underruns.store(0);
    m_sink->start(m_device);

    if (m_sink->error() != QAudio::NoError) {
        emit error(QStringLiteral("Audio output failed to start"));
        stop();
        return false;
    }
    qCInfo(lcAudio) << "audio" << device.description() << f.sampleRate() << "Hz" << f.channelCount() << "ch"
                    << f.sampleFormat() << "buffer" << m_sink->bufferSize() << "bytes";
    emit started(double(f.sampleRate()), device.description());
    return true;
}

void AudioOutput::stop()
{
    if (m_sink) {
        m_sink->stop();
        delete m_sink;
        m_sink = nullptr;
    }
    if (m_device) {
        delete m_device;
        m_device = nullptr;
    }
    m_openedId.clear();
}

}
