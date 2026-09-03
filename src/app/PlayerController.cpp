// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/PlayerController.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QMetaObject>
#include <QSettings>
#include <algorithm>
#include <cmath>

Q_LOGGING_CATEGORY(lcPlayer, "esdr3.player", QtInfoMsg)

namespace esdr3 {

namespace {
constexpr size_t kAudioRingSamples = 1 << 15;
}

PlayerController* PlayerController::create(QQmlEngine*, QJSEngine*)
{
    Q_ASSERT_X(s_instance, "PlayerController::create", "setInstance() must be called before QML loads");
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

PlayerController::PlayerController(QObject* parent)
    : QObject(parent),
      m_tail(std::make_shared<IqTailBuffer>(1 << 17)),
      m_audioRing(std::make_shared<AudioRing>(kAudioRingSamples))
{
    qRegisterMetaType<esdr3::EngineStatus>();
    qRegisterMetaType<esdr3::PsdFrame>();
    qRegisterMetaType<esdr3::DemodParams>();

    setLanguage(QSettings().value(QStringLiteral("ui/language"), QStringLiteral("en")).toString());
    loadDemodSettings();

    m_engine = new PlaybackEngine(m_tail, m_audioRing);
    m_engine->moveToThread(&m_engineThread);
    connect(&m_engineThread, &QThread::finished, m_engine, &QObject::deleteLater);
    connect(m_engine, &PlaybackEngine::opened, this, &PlayerController::onOpened);
    connect(m_engine, &PlaybackEngine::status, this, &PlayerController::onStatus);
    connect(m_engine, &PlaybackEngine::error, this, &PlayerController::onError);
    connect(m_engine, &PlaybackEngine::ended, this, &PlayerController::ended);
    m_engineThread.setObjectName(QStringLiteral("esdr3.engine"));
    m_engineThread.start(QThread::TimeCriticalPriority);
    pushDemod();

    m_spectrum = new SpectrumWorker(m_tail);
    m_spectrum->moveToThread(&m_spectrumThread);
    connect(&m_spectrumThread, &QThread::finished, m_spectrum, &QObject::deleteLater);
    m_spectrumThread.setObjectName(QStringLiteral("esdr3.spectrum"));
    m_spectrumThread.start();
    QMetaObject::invokeMethod(m_spectrum, [this] {
        m_spectrum->setFftSize(m_fftSize);
        m_spectrum->setAveraging(float(m_averaging));
        m_spectrum->setFrameRate(m_frameRate);
        m_spectrum->start();
    });

    m_audioDeviceId = QSettings().value(QStringLiteral("audio/device")).toString();
    m_mediaDevices = new QMediaDevices(this);
    connect(m_mediaDevices, &QMediaDevices::audioOutputsChanged, this, &PlayerController::audioDevicesChanged);

    m_audio = new AudioOutput(m_audioRing);
    m_audio->moveToThread(&m_audioThread);
    connect(&m_audioThread, &QThread::finished, m_audio, &QObject::deleteLater);
    connect(m_audio, &AudioOutput::started, this, &PlayerController::onAudioStarted);
    connect(m_audio, &AudioOutput::error, this, &PlayerController::onError);
    m_audioThread.setObjectName(QStringLiteral("esdr3.audio"));
    m_audioThread.start(QThread::HighPriority);
    const QString devId = m_audioDeviceId;
    QMetaObject::invokeMethod(m_audio, [this, devId] {
        m_audio->setDevice(devId);
        m_audio->start();
    });
}

PlayerController::~PlayerController()
{
    saveDemodSettings();
    QMetaObject::invokeMethod(m_audio, &AudioOutput::stop, Qt::BlockingQueuedConnection);
    QMetaObject::invokeMethod(m_spectrum, &SpectrumWorker::stop, Qt::BlockingQueuedConnection);
    QMetaObject::invokeMethod(m_engine, &PlaybackEngine::shutdown, Qt::BlockingQueuedConnection);
    m_audioThread.quit();
    m_spectrumThread.quit();
    m_engineThread.quit();
    m_audioThread.wait();
    m_spectrumThread.wait();
    m_engineThread.wait();
}

QString PlayerController::version() const { return QCoreApplication::applicationVersion(); }

QStringList PlayerController::languages() const
{
    return {QStringLiteral("en"), QStringLiteral("ru")};
}

QStringList PlayerController::languageNames() const
{
    return {QStringLiteral("English"), QStringLiteral("Русский")};
}

void PlayerController::setLanguage(const QString& codeIn)
{
    const QString code = languages().contains(codeIn) ? codeIn : QStringLiteral("en");
    if (m_translator) {
        QCoreApplication::removeTranslator(m_translator);
        delete m_translator;
        m_translator = nullptr;
    }
    if (code != QLatin1String("en")) {
        auto* tr = new QTranslator(this);
        if (tr->load(QStringLiteral(":/i18n/ESDR3_Player_%1.qm").arg(code))) {
            QCoreApplication::installTranslator(tr);
            m_translator = tr;
        } else {
            qCWarning(lcPlayer) << "translation not found for" << code;
            delete tr;
        }
    }
    QSettings().setValue(QStringLiteral("ui/language"), code);
    const bool changed = m_language != code;
    m_language = code;
    if (changed) emit languageChanged();
}

QString PlayerController::fileName() const
{
    return m_status.path.isEmpty() ? QString() : QFileInfo(m_status.path).fileName();
}

double PlayerController::position() const
{
    return m_status.total ? double(m_status.position) / double(m_status.total) : 0.0;
}

double PlayerController::durationSec() const
{
    return m_status.sampleRate > 0 ? double(m_status.total) / m_status.sampleRate : 0.0;
}

QString PlayerController::formatSeconds(double seconds)
{
    const int s = int(std::floor(std::max(0.0, seconds)));
    const int h = s / 3600, m = (s / 60) % 60, sec = s % 60;
    if (h > 0)
        return QStringLiteral("%1:%2:%3").arg(h).arg(m, 2, 10, QChar('0')).arg(sec, 2, 10, QChar('0'));
    return QStringLiteral("%1:%2").arg(m, 2, 10, QChar('0')).arg(sec, 2, 10, QChar('0'));
}

QString PlayerController::positionText() const
{
    if (m_status.sampleRate <= 0) return QStringLiteral("--:-- / --:--");
    const double pos = double(m_status.position) / m_status.sampleRate;
    return formatSeconds(pos) + QStringLiteral(" / ") + formatSeconds(durationSec());
}

QString PlayerController::recTimeText() const
{
    if (!m_status.recStart.isValid() || m_status.sampleRate <= 0) return QString();
    const double pos = double(m_status.position) / m_status.sampleRate;
    return m_status.recStart.addMSecs(qint64(pos * 1000)).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}

void PlayerController::setLoop(bool on)
{
    QMetaObject::invokeMethod(m_engine, [this, on] { m_engine->setLoop(on); });
}

void PlayerController::setSpeed(double s)
{
    QMetaObject::invokeMethod(m_engine, [this, s] { m_engine->setSpeed(s); });
}

void PlayerController::open(const QUrl& url)
{
    openPath(url.isLocalFile() ? url.toLocalFile() : url.toString());
}

void PlayerController::openPath(const QString& path)
{
    if (path.isEmpty()) return;
    const QString abs = QFileInfo(path).absoluteFilePath();
    qCInfo(lcPlayer) << "open" << abs;
    QMetaObject::invokeMethod(m_engine, [this, abs] { m_engine->open(abs); });
}

void PlayerController::play() { QMetaObject::invokeMethod(m_engine, &PlaybackEngine::play); }
void PlayerController::pause() { QMetaObject::invokeMethod(m_engine, &PlaybackEngine::pause); }
void PlayerController::togglePlay() { QMetaObject::invokeMethod(m_engine, &PlaybackEngine::togglePlay); }
void PlayerController::stop() { QMetaObject::invokeMethod(m_engine, &PlaybackEngine::stop); }

void PlayerController::seek(double fraction)
{
    QMetaObject::invokeMethod(m_engine, [this, fraction] { m_engine->seek(fraction); });
    QMetaObject::invokeMethod(m_spectrum, &SpectrumWorker::reset);
}

void PlayerController::seekBy(double seconds)
{
    const double dur = durationSec();
    if (dur <= 0) return;
    const double pos = double(m_status.position) / m_status.sampleRate + seconds;
    seek(std::clamp(pos / dur, 0.0, 1.0));
}

void PlayerController::setFftSize(int n)
{
    if (m_fftSize == n) return;
    m_fftSize = n;
    QMetaObject::invokeMethod(m_spectrum, [this, n] { m_spectrum->setFftSize(n); });
    emit spectrumSettingsChanged();
}

void PlayerController::setAveraging(double a)
{
    if (m_averaging == a) return;
    m_averaging = a;
    QMetaObject::invokeMethod(m_spectrum, [this, a] { m_spectrum->setAveraging(float(a)); });
    emit spectrumSettingsChanged();
}

void PlayerController::setFrameRate(int fps)
{
    if (m_frameRate == fps) return;
    m_frameRate = fps;
    QMetaObject::invokeMethod(m_spectrum, [this, fps] { m_spectrum->setFrameRate(fps); });
    emit spectrumSettingsChanged();
}

double PlayerController::defaultBandwidth(Mode m)
{
    switch (m) {
    case Mode::CW: return 500;
    case Mode::USB:
    case Mode::LSB: return 2700;
    case Mode::AM: return 8000;
    case Mode::FM: return 12000;
    }
    return 2700;
}

double PlayerController::bandwidthMin() const
{
    switch (m_demod.mode) {
    case Mode::CW: return 50;
    case Mode::USB:
    case Mode::LSB: return 1000;
    case Mode::AM: return 3000;
    case Mode::FM: return 6000;
    }
    return 100;
}

double PlayerController::bandwidthMax() const
{
    switch (m_demod.mode) {
    case Mode::CW: return 1500;
    case Mode::USB:
    case Mode::LSB: return 4000;
    case Mode::AM: return 12000;
    case Mode::FM: return 16000;
    }
    return 4000;
}

double PlayerController::filterLowHz() const
{
    float lo, hi;
    Demodulator::filterEdges(m_demod, lo, hi);
    return lo;
}

double PlayerController::filterHighHz() const
{
    float lo, hi;
    Demodulator::filterEdges(m_demod, lo, hi);
    return hi;
}

int PlayerController::underruns() const { return m_audio ? m_audio->underruns() : 0; }

QStringList PlayerController::audioDeviceIds() const
{
    QStringList ids{QString()};
    for (const auto& d : QMediaDevices::audioOutputs()) ids << QString::fromLatin1(d.id());
    return ids;
}

QStringList PlayerController::audioDeviceNames() const
{
    QStringList names{tr("System default")};
    for (const auto& d : QMediaDevices::audioOutputs()) names << d.description();
    return names;
}

void PlayerController::setAudioDeviceId(const QString& id)
{
    if (id == m_audioDeviceId) return;
    m_audioDeviceId = id;
    QSettings().setValue(QStringLiteral("audio/device"), id);
    QMetaObject::invokeMethod(m_audio, [this, id] { m_audio->setDevice(id); });
    emit audioDevicesChanged();
}

void PlayerController::setFilterEdges(double lowHz, double highHz)
{
    switch (m_demod.mode) {
    case Mode::USB: setBandwidthHz(highHz - 200.0); break;
    case Mode::LSB: setBandwidthHz(-lowHz - 200.0); break;
    default: setBandwidthHz(highHz - lowHz); break;
    }
}

void PlayerController::pushDemod()
{
    const DemodParams p = m_demod;
    QMetaObject::invokeMethod(m_engine, [this, p] { m_engine->setDemod(p); });
    emit demodChanged();
}

void PlayerController::setMode(int m)
{
    const Mode mode = Mode(std::clamp(m, 0, 4));
    if (mode == m_demod.mode) return;
    m_bwByMode[size_t(m_demod.mode)] = m_demod.bandwidthHz;
    m_demod.mode = mode;
    m_demod.bandwidthHz = float(m_bwByMode[size_t(mode)]);
    pushDemod();
    saveDemodSettings();
}

void PlayerController::setVfoHz(double hz)
{
    if (m_status.sampleRate > 0) {
        const double half = m_status.sampleRate / 2;
        hz = std::clamp(hz, m_status.ddsHz - half, m_status.ddsHz + half);
    }
    const double tune = hz - m_status.ddsHz;
    if (tune == m_demod.tuneHz) return;
    m_demod.tuneHz = tune;
    pushDemod();
}

void PlayerController::tuneBy(double hz) { setVfoHz(vfoHz() + hz); }

void PlayerController::setBandwidthHz(double hz)
{
    hz = std::clamp(hz, bandwidthMin(), bandwidthMax());
    if (float(hz) == m_demod.bandwidthHz) return;
    m_demod.bandwidthHz = float(hz);
    m_bwByMode[size_t(m_demod.mode)] = hz;
    pushDemod();
    saveDemodSettings();
}

void PlayerController::setCwPitchHz(double hz)
{
    hz = std::clamp(hz, 300.0, 1200.0);
    if (float(hz) == m_demod.cwPitchHz) return;
    m_demod.cwPitchHz = float(hz);
    pushDemod();
    saveDemodSettings();
}

void PlayerController::setAgcMode(int a)
{
    const AgcMode agc = AgcMode(std::clamp(a, 0, 2));
    if (agc == m_demod.agc) return;
    m_demod.agc = agc;
    pushDemod();
    saveDemodSettings();
}

void PlayerController::setVolume(double v)
{
    v = std::clamp(v, 0.0, 1.0);
    if (float(v) == m_demod.volume) return;
    m_demod.volume = float(v);
    pushDemod();
    saveDemodSettings();
}

void PlayerController::setMute(bool on)
{
    if (on == m_demod.mute) return;
    m_demod.mute = on;
    pushDemod();
}

void PlayerController::loadDemodSettings()
{
    QSettings s;
    s.beginGroup(QStringLiteral("demod"));
    m_demod.mode = Mode(std::clamp(s.value(QStringLiteral("mode"), 0).toInt(), 0, 4));
    m_demod.cwPitchHz = float(s.value(QStringLiteral("cwPitchHz"), 700.0).toDouble());
    m_demod.agc = AgcMode(std::clamp(s.value(QStringLiteral("agc"), 2).toInt(), 0, 2));
    m_demod.volume = float(s.value(QStringLiteral("volume"), 0.5).toDouble());
    for (size_t i = 0; i < m_bwByMode.size(); ++i)
        m_bwByMode[i] = s.value(QStringLiteral("bandwidth%1").arg(i), defaultBandwidth(Mode(i))).toDouble();
    m_demod.bandwidthHz = float(m_bwByMode[size_t(m_demod.mode)]);
}

void PlayerController::saveDemodSettings() const
{
    QSettings s;
    s.beginGroup(QStringLiteral("demod"));
    s.setValue(QStringLiteral("mode"), int(m_demod.mode));
    s.setValue(QStringLiteral("cwPitchHz"), double(m_demod.cwPitchHz));
    s.setValue(QStringLiteral("agc"), int(m_demod.agc));
    s.setValue(QStringLiteral("volume"), double(m_demod.volume));
    for (size_t i = 0; i < m_bwByMode.size(); ++i)
        s.setValue(QStringLiteral("bandwidth%1").arg(i), m_bwByMode[i]);
}

void PlayerController::onOpened(const EngineStatus& s)
{
    qCInfo(lcPlayer) << "opened" << s.path << "fs" << s.sampleRate << "dds" << s.ddsHz << "samples" << s.total;
    m_status = s;
    m_lastError.clear();
    m_demod.tuneHz = 0;
    QMetaObject::invokeMethod(m_spectrum, [this, s] {
        m_spectrum->setSampleRate(s.sampleRate);
        m_spectrum->setCenterHz(s.ddsHz);
        m_spectrum->reset();
    });
    pushDemod();
    emit errorChanged();
    emit fileChanged();
    emit statusChanged();
}

void PlayerController::onStatus(const EngineStatus& s)
{
    const bool fileChangedNow = s.path != m_status.path || s.sampleRate != m_status.sampleRate ||
                                s.ddsHz != m_status.ddsHz;
    m_status = s;
    if (fileChangedNow) emit fileChanged();
    emit statusChanged();
}

void PlayerController::onError(const QString& message)
{
    qCWarning(lcPlayer) << message;
    m_lastError = message;
    emit errorChanged();
}

void PlayerController::onAudioStarted(double rate, const QString& description)
{
    m_audioRate = rate;
    m_audioDevice = description;
    QMetaObject::invokeMethod(m_engine, [this, rate] { m_engine->setAudioRate(rate); });
    emit audioChanged();
}

}
