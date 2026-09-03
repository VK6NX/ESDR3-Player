// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "audio/AudioOutput.h"
#include "core/PlaybackEngine.h"
#include "core/SpscRing.h"
#include "core/TailBuffer.h"
#include "core/Types.h"
#include "dsp/SpectrumWorker.h"

#include <QJSEngine>
#include <QMediaDevices>
#include <QObject>
#include <QQmlEngine>
#include <QThread>
#include <QTranslator>
#include <QUrl>
#include <QtQml/qqmlregistration.h>
#include <array>
#include <memory>

namespace esdr3 {

class PlayerController : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(Player)
    QML_SINGLETON

    Q_PROPERTY(int state READ state NOTIFY statusChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY statusChanged)
    Q_PROPERTY(bool hasFile READ hasFile NOTIFY statusChanged)
    Q_PROPERTY(QString fileName READ fileName NOTIFY fileChanged)
    Q_PROPERTY(QString filePath READ filePath NOTIFY fileChanged)
    Q_PROPERTY(double position READ position NOTIFY statusChanged)
    Q_PROPERTY(QString positionText READ positionText NOTIFY statusChanged)
    Q_PROPERTY(QString recTimeText READ recTimeText NOTIFY statusChanged)
    Q_PROPERTY(QDateTime recStart READ recStart NOTIFY fileChanged)
    Q_PROPERTY(double ddsHz READ ddsHz NOTIFY fileChanged)
    Q_PROPERTY(double sampleRate READ sampleRate NOTIFY fileChanged)
    Q_PROPERTY(double durationSec READ durationSec NOTIFY statusChanged)
    Q_PROPERTY(bool loop READ loop WRITE setLoop NOTIFY statusChanged)
    Q_PROPERTY(double speed READ speed WRITE setSpeed NOTIFY statusChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY errorChanged)

    Q_PROPERTY(int fftSize READ fftSize WRITE setFftSize NOTIFY spectrumSettingsChanged)
    Q_PROPERTY(double averaging READ averaging WRITE setAveraging NOTIFY spectrumSettingsChanged)
    Q_PROPERTY(int frameRate READ frameRate WRITE setFrameRate NOTIFY spectrumSettingsChanged)
    Q_PROPERTY(QObject* spectrum READ spectrum CONSTANT)

    Q_PROPERTY(int mode READ mode WRITE setMode NOTIFY demodChanged)
    Q_PROPERTY(double vfoHz READ vfoHz WRITE setVfoHz NOTIFY demodChanged)
    Q_PROPERTY(double bandwidthHz READ bandwidthHz WRITE setBandwidthHz NOTIFY demodChanged)
    Q_PROPERTY(double cwPitchHz READ cwPitchHz WRITE setCwPitchHz NOTIFY demodChanged)
    Q_PROPERTY(int agcMode READ agcMode WRITE setAgcMode NOTIFY demodChanged)
    Q_PROPERTY(double volume READ volume WRITE setVolume NOTIFY demodChanged)
    Q_PROPERTY(bool mute READ mute WRITE setMute NOTIFY demodChanged)
    Q_PROPERTY(double filterLowHz READ filterLowHz NOTIFY demodChanged)
    Q_PROPERTY(double filterHighHz READ filterHighHz NOTIFY demodChanged)
    Q_PROPERTY(double bandwidthMin READ bandwidthMin NOTIFY demodChanged)
    Q_PROPERTY(double bandwidthMax READ bandwidthMax NOTIFY demodChanged)
    Q_PROPERTY(double audioRate READ audioRate NOTIFY audioChanged)
    Q_PROPERTY(QString audioDevice READ audioDevice NOTIFY audioChanged)
    Q_PROPERTY(int underruns READ underruns NOTIFY statusChanged)
    Q_PROPERTY(QStringList audioDeviceIds READ audioDeviceIds NOTIFY audioDevicesChanged)
    Q_PROPERTY(QStringList audioDeviceNames READ audioDeviceNames NOTIFY audioDevicesChanged)
    Q_PROPERTY(QString audioDeviceId READ audioDeviceId WRITE setAudioDeviceId NOTIFY audioDevicesChanged)

    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(QStringList languages READ languages CONSTANT)
    Q_PROPERTY(QStringList languageNames READ languageNames CONSTANT)
    Q_PROPERTY(QString version READ version CONSTANT)

public:
    QString version() const;
    explicit PlayerController(QObject* parent);
    ~PlayerController() override;

    static void setInstance(PlayerController* instance) { s_instance = instance; }
    static PlayerController* create(QQmlEngine*, QJSEngine*);

    int state() const { return int(m_status.state); }
    bool playing() const { return m_status.state == PlayState::Playing; }
    bool hasFile() const { return m_status.state != PlayState::Idle; }
    QString fileName() const;
    QString filePath() const { return m_status.path; }
    double position() const;
    QString positionText() const;
    QString recTimeText() const;
    QDateTime recStart() const { return m_status.recStart; }
    double ddsHz() const { return m_status.ddsHz; }
    double sampleRate() const { return m_status.sampleRate; }
    double durationSec() const;
    bool loop() const { return m_status.loop; }
    void setLoop(bool on);
    double speed() const { return m_status.speed; }
    void setSpeed(double s);
    QString lastError() const { return m_lastError; }

    int fftSize() const { return m_fftSize; }
    void setFftSize(int n);
    double averaging() const { return m_averaging; }
    void setAveraging(double a);
    int frameRate() const { return m_frameRate; }
    void setFrameRate(int fps);
    QObject* spectrum() const { return m_spectrum; }

    int mode() const { return int(m_demod.mode); }
    void setMode(int m);
    double vfoHz() const { return m_status.ddsHz + m_demod.tuneHz; }
    void setVfoHz(double hz);
    double bandwidthHz() const { return m_demod.bandwidthHz; }
    void setBandwidthHz(double hz);
    double cwPitchHz() const { return m_demod.cwPitchHz; }
    void setCwPitchHz(double hz);
    int agcMode() const { return int(m_demod.agc); }
    void setAgcMode(int a);
    double volume() const { return m_demod.volume; }
    void setVolume(double v);
    bool mute() const { return m_demod.mute; }
    void setMute(bool on);
    double filterLowHz() const;
    double filterHighHz() const;
    double bandwidthMin() const;
    double bandwidthMax() const;
    double audioRate() const { return m_audioRate; }
    QString audioDevice() const { return m_audioDevice; }
    int underruns() const;
    QStringList audioDeviceIds() const;
    QStringList audioDeviceNames() const;
    QString audioDeviceId() const { return m_audioDeviceId; }
    void setAudioDeviceId(const QString& id);

    QString language() const { return m_language; }
    void setLanguage(const QString& code);
    QStringList languages() const;
    QStringList languageNames() const;

    Q_INVOKABLE void open(const QUrl& url);
    Q_INVOKABLE void openPath(const QString& path);
    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void togglePlay();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void seek(double fraction);
    Q_INVOKABLE void seekBy(double seconds);
    Q_INVOKABLE void tuneBy(double hz);
    Q_INVOKABLE void setFilterEdges(double lowHz, double highHz);
    Q_INVOKABLE static QString formatSeconds(double seconds);

signals:
    void statusChanged();
    void fileChanged();
    void spectrumSettingsChanged();
    void demodChanged();
    void audioChanged();
    void audioDevicesChanged();
    void errorChanged();
    void ended();
    void languageChanged();

private slots:
    void onOpened(const esdr3::EngineStatus& s);
    void onStatus(const esdr3::EngineStatus& s);
    void onError(const QString& message);
    void onAudioStarted(double rate, const QString& description);

private:
    void pushDemod();
    void loadDemodSettings();
    void saveDemodSettings() const;
    static double defaultBandwidth(Mode m);

    static inline PlayerController* s_instance = nullptr;

    std::shared_ptr<IqTailBuffer> m_tail;
    std::shared_ptr<AudioRing> m_audioRing;
    QThread m_engineThread;
    QThread m_spectrumThread;
    QThread m_audioThread;
    PlaybackEngine* m_engine = nullptr;
    SpectrumWorker* m_spectrum = nullptr;
    AudioOutput* m_audio = nullptr;
    EngineStatus m_status;
    DemodParams m_demod;
    std::array<double, 5> m_bwByMode{500, 2700, 2700, 8000, 12000};
    double m_audioRate = 0;
    QString m_audioDevice;
    QString m_audioDeviceId;
    QMediaDevices* m_mediaDevices = nullptr;
    QString m_lastError;
    int m_fftSize = 16384;
    double m_averaging = 0.5;
    int m_frameRate = 25;
    QString m_language = QStringLiteral("en");
    QTranslator* m_translator = nullptr;
};

}
