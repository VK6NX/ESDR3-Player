// SPDX-License-Identifier: GPL-3.0-or-later
// esdr3_probe: консольная проверка ядра без GUI.
//   esdr3_probe <file.wav> [seconds=2] [fft=16384] [peaks=10]
// Печатает метаданные, читает первые N секунд, считает усреднённый спектр и
// выводит самые сильные пики в абсолютных частотах для сверки с эталоном.
#include "core/WavReader.h"
#include "dsp/PsdComputer.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace esdr3;

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    const QStringList args = app.arguments();
    if (args.size() < 2) {
        std::fprintf(stderr, "usage: esdr3_probe <file.wav> [seconds] [fft] [peaks]\n");
        return 2;
    }
    const double seconds = args.size() > 2 ? args[2].toDouble() : 2.0;
    const int fft = args.size() > 3 ? args[3].toInt() : 16384;
    const int npeaks = args.size() > 4 ? args[4].toInt() : 10;

    WavReader r;
    QString err;
    if (!r.open(args[1], &err)) {
        std::fprintf(stderr, "open failed: %s\n", qPrintable(err));
        return 1;
    }
    const auto& f = r.format();
    const auto& m = r.esdr();
    const double fs = r.sampleRate();
    std::printf("file        %s\n", qPrintable(r.path()));
    std::printf("fmt         tag=%u ch=%u bits=%u align=%u rate=%u float=%d data@%llu\n",
                f.formatTag, f.channels, f.bits, f.blockAlign, f.fmtRate, int(f.isFloat),
                (unsigned long long)f.dataOffset);
    std::printf("esdr        present=%d ver=%u dds=%.1f fs=%.3f start=%s stop=%s\n",
                int(m.present), m.version, m.ddsHz, m.sampleRate,
                qPrintable(m.start.toString(Qt::ISODate)), qPrintable(m.stop.toString(Qt::ISODate)));
    std::printf("effective   fs=%.3f dds=%.1f samples=%llu duration=%.1f s\n", fs, r.ddsHz(),
                (unsigned long long)r.totalSamples(), double(r.totalSamples()) / fs);

    PsdComputer psd(fft);
    const size_t N = static_cast<size_t>(fft);
    std::vector<std::complex<float>> buf(N);
    std::vector<float> db(N), avg(N, 0.0f);
    uint64_t want = uint64_t(seconds * fs);
    int frames = 0;
    double peakAbs = 0;
    QElapsedTimer t;
    t.start();
    while (r.position() + uint64_t(fft) <= std::min<uint64_t>(want, r.totalSamples())) {
        size_t got = 0;
        while (got < size_t(fft)) {
            size_t n = r.read(buf.data() + got, size_t(fft) - got);
            if (n == 0) break;
            got += n;
        }
        if (got < size_t(fft)) break;
        for (auto& v : buf) peakAbs = std::max(peakAbs, double(std::max(std::abs(v.real()), std::abs(v.imag()))));
        psd.compute(buf.data(), db.data());
        for (size_t i = 0; i < size_t(fft); ++i) avg[i] += db[i];
        ++frames;
    }
    if (frames == 0) {
        std::fprintf(stderr, "not enough samples\n");
        return 1;
    }
    for (auto& v : avg) v /= float(frames);
    const double ms = t.elapsed();
    std::printf("psd         frames=%d fft=%d rbw=%.2f Hz time=%.1f ms (%.2f ms/frame) peak|x|=%.4f\n",
                frames, fft, fs / fft, ms, ms / frames, peakAbs);

    // Пол шума: медиана.
    std::vector<float> sorted(avg);
    std::nth_element(sorted.begin(), sorted.begin() + sorted.size() / 2, sorted.end());
    const float floorDb = sorted[sorted.size() / 2];
    std::printf("noise floor %.1f dBFS (median)\n", floorDb);

    // Локальные максимумы, отсортированные по уровню.
    std::vector<int> idx;
    for (int k = 2; k < fft - 2; ++k)
        if (avg[size_t(k)] > avg[size_t(k) - 1] && avg[size_t(k)] >= avg[size_t(k) + 1] &&
            avg[size_t(k)] > avg[size_t(k) - 2] && avg[size_t(k)] >= avg[size_t(k) + 2])
            idx.push_back(k);
    std::sort(idx.begin(), idx.end(), [&](int a, int b) { return avg[size_t(a)] > avg[size_t(b)]; });
    std::printf("top peaks   (abs Hz, offset Hz, dBFS, SNR dB)\n");
    for (int i = 0; i < npeaks && i < int(idx.size()); ++i) {
        const int k = idx[size_t(i)];
        const double off = (k - fft / 2) * fs / fft;
        std::printf("  %12.0f  %+9.1f  %7.1f  %6.1f\n", r.ddsHz() + off, off, avg[size_t(k)], avg[size_t(k)] - floorDb);
    }
    return 0;
}
