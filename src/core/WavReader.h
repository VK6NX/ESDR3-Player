// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDateTime>
#include <QFile>
#include <QIODevice>
#include <QString>
#include <complex>
#include <cstdint>
#include <vector>

namespace esdr3 {

struct Esdr3Meta {
    bool present = false;
    uint32_t version = 0;
    double ddsHz = 0;
    double sampleRate = 0;
    QDateTime start;
    QDateTime stop;
};

struct WavFormat {
    uint16_t formatTag = 0;
    uint16_t channels = 0;
    uint16_t bits = 0;
    uint16_t blockAlign = 0;
    uint32_t fmtRate = 0;
    bool isFloat = false;
    uint64_t dataOffset = 0;
    uint64_t dataBytes = 0;
};

class WavReader {
public:
    WavReader() = default;
    ~WavReader();
    WavReader(const WavReader&) = delete;
    WavReader& operator=(const WavReader&) = delete;

    bool open(const QString& path, QString* error = nullptr);
    void close();
    bool isOpen() const { return m_file.isOpen(); }
    const QString& path() const { return m_path; }

    const WavFormat& format() const { return m_fmt; }
    const Esdr3Meta& esdr() const { return m_meta; }

    double sampleRate() const;
    double ddsHz() const;
    QDateTime startTime() const;

    uint64_t totalSamples() const { return m_fmt.blockAlign ? m_fmt.dataBytes / m_fmt.blockAlign : 0; }
    bool refreshLength();
    uint64_t position() const { return m_pos; }
    bool seek(uint64_t sample);

    size_t read(std::complex<float>* out, size_t maxSamples);

    static bool parseHeader(QIODevice& dev, WavFormat& fmt, Esdr3Meta& meta, QString* error);

    static bool metaFromFileName(const QString& fileName, double* ddsHz, QDateTime* start);

private:
    void convert(const uint8_t* src, std::complex<float>* out, size_t n) const;

    QFile m_file;
    QString m_path;
    WavFormat m_fmt;
    Esdr3Meta m_meta;
    double m_nameDds = 0;
    QDateTime m_nameStart;
    uint64_t m_pos = 0;
    std::vector<uint8_t> m_buf;
};

}
