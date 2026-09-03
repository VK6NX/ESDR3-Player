// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/WavReader.h"

#include <QFileInfo>
#include <QRegularExpression>
#include <QTimeZone>
#include <QtEndian>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace esdr3 {

namespace {

uint16_t rd16(const uint8_t* p) { return qFromLittleEndian<quint16>(p); }
uint32_t rd32(const uint8_t* p) { return qFromLittleEndian<quint32>(p); }

double rdDouble(const uint8_t* p)
{
    quint64 bits = qFromLittleEndian<quint64>(p);
    double d;
    std::memcpy(&d, &bits, sizeof d);
    return d;
}

QDateTime rdSystemTime(const uint8_t* p)
{
    QDate date(rd16(p), rd16(p + 2), rd16(p + 6));
    QTime time(rd16(p + 8), rd16(p + 10), rd16(p + 12), rd16(p + 14));
    if (!date.isValid() || !time.isValid()) return {};
    return QDateTime(date, time, QTimeZone::UTC);
}

bool finitePositive(double v) { return std::isfinite(v) && v > 0; }

constexpr size_t kReadChunkSamples = 1 << 14;

}

WavReader::~WavReader() { close(); }

bool WavReader::parseHeader(QIODevice& dev, WavFormat& fmt, Esdr3Meta& meta, QString* error)
{
    auto fail = [&](const QString& msg) {
        if (error) *error = msg;
        return false;
    };

    fmt = WavFormat{};
    meta = Esdr3Meta{};

    uint8_t riff[12];
    if (dev.read(reinterpret_cast<char*>(riff), 12) != 12) return fail("file is too short");
    if (std::memcmp(riff, "RIFF", 4) != 0 || std::memcmp(riff + 8, "WAVE", 4) != 0)
        return fail("not a RIFF/WAVE file");

    bool haveFmt = false;
    for (;;) {
        uint8_t ch[8];
        if (dev.read(reinterpret_cast<char*>(ch), 8) != 8) return fail("no data chunk");
        const uint32_t size = rd32(ch + 4);
        const qint64 body = dev.pos();

        if (std::memcmp(ch, "fmt ", 4) == 0) {
            uint8_t b[40] = {0};
            const int n = int(std::min<uint32_t>(size, sizeof b));
            if (n < 16 || dev.read(reinterpret_cast<char*>(b), n) != n) return fail("fmt chunk is truncated");
            fmt.formatTag = rd16(b);
            fmt.channels = rd16(b + 2);
            fmt.fmtRate = rd32(b + 4);
            fmt.blockAlign = rd16(b + 12);
            fmt.bits = rd16(b + 14);
            uint16_t tag = fmt.formatTag;
            if (tag == 0xFFFE && n >= 26) tag = rd16(b + 24);
            if (tag != 1 && tag != 3) return fail(QString("unsupported WAV format tag %1").arg(tag));
            fmt.isFloat = (tag == 3);
            haveFmt = true;
        } else if (std::memcmp(ch, "esdr", 4) == 0) {
            uint8_t b[64] = {0};
            const int n = int(std::min<uint32_t>(size, sizeof b));
            if (dev.read(reinterpret_cast<char*>(b), n) != n) return fail("esdr chunk is truncated");
            meta.present = true;
            meta.version = rd32(b);
            if (n >= 36) {
                meta.start = rdSystemTime(b + 4);
                meta.stop = rdSystemTime(b + 20);
            }
            if (n >= 56) {
                meta.ddsHz = rdDouble(b + 40);
                meta.sampleRate = rdDouble(b + 48);
            }
            if (!finitePositive(meta.ddsHz)) meta.ddsHz = 0;
            if (!finitePositive(meta.sampleRate)) meta.sampleRate = 0;
        } else if (std::memcmp(ch, "data", 4) == 0) {
            fmt.dataOffset = uint64_t(body);
            break;
        }

        const qint64 next = body + qint64(size) + (size & 1);
        if (!dev.seek(next)) return fail("chunk extends beyond end of file");
    }

    if (!haveFmt) return fail("no fmt chunk before data");
    if (fmt.channels < 1) return fail("zero channels");
    if (fmt.bits != 8 && fmt.bits != 16 && fmt.bits != 24 && fmt.bits != 32)
        return fail(QString("unsupported sample size %1 bit").arg(fmt.bits));
    if (fmt.isFloat && fmt.bits != 32) return fail("float samples must be 32 bit");
    const uint16_t expectAlign = uint16_t(fmt.channels * fmt.bits / 8);
    if (fmt.blockAlign == 0) fmt.blockAlign = expectAlign;
    if (fmt.blockAlign != expectAlign) return fail("blockAlign does not match channels and bits");
    if (fmt.fmtRate == 0 && meta.sampleRate == 0) return fail("sample rate is zero");
    return true;
}

bool WavReader::metaFromFileName(const QString& fileName, double* ddsHz, QDateTime* start)
{
    static const QRegularExpression reFreq(QStringLiteral("Freq_(\\d+)_Hz"));
    static const QRegularExpression reTime(
        QStringLiteral("Date_(\\d{2})-(\\d{2})-(\\d{4})_Time_(\\d{2})-(\\d{2})-(\\d{2})"));

    bool any = false;
    const QString name = QFileInfo(fileName).fileName();

    if (auto m = reFreq.match(name); m.hasMatch()) {
        if (ddsHz) *ddsHz = m.captured(1).toDouble();
        any = true;
    }
    if (auto m = reTime.match(name); m.hasMatch()) {
        QDate d(m.captured(3).toInt(), m.captured(2).toInt(), m.captured(1).toInt());
        QTime t(m.captured(4).toInt(), m.captured(5).toInt(), m.captured(6).toInt());
        if (d.isValid() && t.isValid()) {
            if (start) *start = QDateTime(d, t, QTimeZone::UTC);
            any = true;
        }
    }
    return any;
}

bool WavReader::open(const QString& path, QString* error)
{
    close();
    m_file.setFileName(path);
    if (!m_file.open(QIODevice::ReadOnly)) {
        if (error) *error = m_file.errorString();
        return false;
    }
    if (!parseHeader(m_file, m_fmt, m_meta, error)) {
        m_file.close();
        return false;
    }
    m_path = path;
    m_nameDds = 0;
    m_nameStart = QDateTime();
    metaFromFileName(path, &m_nameDds, &m_nameStart);
    refreshLength();
    m_pos = 0;
    m_file.seek(qint64(m_fmt.dataOffset));
    return true;
}

void WavReader::close()
{
    if (m_file.isOpen()) m_file.close();
    m_path.clear();
    m_fmt = WavFormat{};
    m_meta = Esdr3Meta{};
    m_pos = 0;
}

double WavReader::sampleRate() const
{
    return m_meta.sampleRate > 0 ? m_meta.sampleRate : double(m_fmt.fmtRate);
}

double WavReader::ddsHz() const
{
    return m_meta.ddsHz > 0 ? m_meta.ddsHz : m_nameDds;
}

QDateTime WavReader::startTime() const
{
    return m_meta.start.isValid() ? m_meta.start : m_nameStart;
}

bool WavReader::refreshLength()
{
    if (!m_file.isOpen()) return false;
    const qint64 size = m_file.size();
    uint64_t bytes = 0;
    if (size > 0 && uint64_t(size) > m_fmt.dataOffset)
        bytes = (uint64_t(size) - m_fmt.dataOffset) / m_fmt.blockAlign * m_fmt.blockAlign;
    const bool changed = bytes != m_fmt.dataBytes;
    m_fmt.dataBytes = bytes;
    return changed;
}

bool WavReader::seek(uint64_t sample)
{
    if (!m_file.isOpen()) return false;
    const uint64_t total = totalSamples();
    if (sample > total) sample = total;
    if (!m_file.seek(qint64(m_fmt.dataOffset + sample * m_fmt.blockAlign))) return false;
    m_pos = sample;
    return true;
}

size_t WavReader::read(std::complex<float>* out, size_t maxSamples)
{
    if (!m_file.isOpen() || maxSamples == 0) return 0;
    const uint64_t total = totalSamples();
    if (m_pos >= total) return 0;
    size_t n = size_t(std::min<uint64_t>(maxSamples, total - m_pos));
    n = std::min(n, kReadChunkSamples);

    const size_t bytes = n * m_fmt.blockAlign;
    if (m_buf.size() < bytes) m_buf.resize(bytes);
    const qint64 got = m_file.read(reinterpret_cast<char*>(m_buf.data()), qint64(bytes));
    if (got <= 0) return 0;
    n = size_t(got) / m_fmt.blockAlign;
    convert(m_buf.data(), out, n);
    m_pos += n;
    return n;
}

void WavReader::convert(const uint8_t* src, std::complex<float>* out, size_t n) const
{
    const size_t stride = m_fmt.blockAlign;
    const bool stereo = m_fmt.channels >= 2;
    const size_t bps = m_fmt.bits / 8;

    auto sampleAt = [&](const uint8_t* p) -> float {
        switch (m_fmt.bits) {
        case 8:
            return (int(p[0]) - 128) * (1.0f / 128.0f);
        case 16:
            return int16_t(rd16(p)) * (1.0f / 32768.0f);
        case 24: {
            int32_t v = int32_t(p[0]) | (int32_t(p[1]) << 8) | (int32_t(p[2]) << 16);
            if (v & 0x800000) v |= ~0xFFFFFF;
            return float(v) * (1.0f / 8388608.0f);
        }
        case 32:
            if (m_fmt.isFloat) {
                uint32_t bits = rd32(p);
                float f;
                std::memcpy(&f, &bits, sizeof f);
                return f;
            }
            return float(int32_t(rd32(p))) * (1.0f / 2147483648.0f);
        }
        return 0.0f;
    };

    for (size_t i = 0; i < n; ++i, src += stride) {
        const float re = sampleAt(src);
        const float im = stereo ? sampleAt(src + bps) : 0.0f;
        out[i] = std::complex<float>(re, im);
    }
}

}
