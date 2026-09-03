// SPDX-License-Identifier: GPL-3.0-or-later
#include "dsp/Demodulator.h"
#include "dsp/PsdComputer.h"

#include <QtTest>
#include <cmath>
#include <complex>
#include <vector>

using namespace esdr3;

namespace {

std::vector<std::complex<float>> tone(double fs, double f, float a, size_t n, double phase0 = 0)
{
    std::vector<std::complex<float>> x(n);
    for (size_t i = 0; i < n; ++i) {
        const double ph = phase0 + 2 * M_PI * f * double(i) / fs;
        x[i] = std::complex<float>(a * float(std::cos(ph)), a * float(std::sin(ph)));
    }
    return x;
}

double peakHz(const std::vector<float>& audio, double rate, int fft)
{
    const size_t N = static_cast<size_t>(fft);
    std::vector<std::complex<float>> x(N);
    const size_t start = audio.size() - N;
    for (int i = 0; i < fft; ++i) x[size_t(i)] = {audio[start + size_t(i)], 0.0f};
    std::vector<float> db(N);
    PsdComputer psd(fft);
    psd.compute(x.data(), db.data());
    int best = fft / 2 + 1;
    for (int k = fft / 2 + 1; k < fft; ++k)
        if (db[size_t(k)] > db[size_t(best)]) best = k;
    return (best - fft / 2) * rate / fft;
}

std::vector<float> run(Demodulator& d, const std::vector<std::complex<float>>& in, double fs, double audioRate)
{
    std::vector<float> out;
    std::vector<float> buf(size_t(4096 * audioRate / fs) + 64);
    for (size_t pos = 0; pos < in.size(); pos += 4096) {
        const size_t n = std::min<size_t>(4096, in.size() - pos);
        const size_t got = d.process(in.data() + pos, n, buf.data(), buf.size());
        out.insert(out.end(), buf.begin(), buf.begin() + qsizetype(got));
    }
    return out;
}

}

class TestDemod : public QObject {
    Q_OBJECT
private slots:
    void cwToneLandsOnPitch();
    void usbPassesUpperOnly();
    void lsbPassesLowerOnly();
    void filterEdges();
};

void TestDemod::cwToneLandsOnPitch()
{
    const double fs = 39062.5, audio = 48000;
    Demodulator d;
    d.configure(fs, audio);
    DemodParams p;
    p.mode = Mode::CW;
    p.tuneHz = -7202.6;
    p.cwPitchHz = 700;
    p.bandwidthHz = 500;
    p.agc = AgcMode::Slow;
    p.volume = 1.0f;
    d.setParams(p);

    auto in = tone(fs, p.tuneHz, 0.001f, size_t(fs * 2));
    auto out = run(d, in, fs, audio);
    QVERIFY(out.size() > audio * 1.5);
    const double f = peakHz(out, audio, 8192);
    QVERIFY2(std::abs(f - 700) < 12, qPrintable(QString::number(f)));

    float peak = 0;
    for (size_t i = out.size() / 2; i < out.size(); ++i) peak = std::max(peak, std::abs(out[i]));
    QVERIFY2(peak > 0.05f && peak < 0.95f, qPrintable(QString::number(peak)));
}

void TestDemod::usbPassesUpperOnly()
{
    const double fs = 78125, audio = 48000;
    Demodulator d;
    d.configure(fs, audio);
    DemodParams p;
    p.mode = Mode::USB;
    p.tuneHz = 5000;
    p.bandwidthHz = 2700;
    p.agc = AgcMode::Off;
    p.volume = 1.0f;
    d.setParams(p);

    auto upper = tone(fs, p.tuneHz + 1000, 0.0005f, size_t(fs * 1.5));
    auto lower = tone(fs, p.tuneHz - 1000, 0.0005f, size_t(fs * 1.5));
    auto outU = run(d, upper, fs, audio);
    d.reset();
    auto outL = run(d, lower, fs, audio);

    auto rms = [](const std::vector<float>& v) {
        double s = 0;
        for (size_t i = v.size() / 2; i < v.size(); ++i) s += double(v[i]) * v[i];
        return std::sqrt(s / double(v.size() / 2));
    };
    QVERIFY2(std::abs(peakHz(outU, audio, 8192) - 1000) < 12, "USB tone frequency");
    const double ratioDb = 20 * std::log10(rms(outU) / std::max(1e-9, rms(outL)));
    QVERIFY2(ratioDb > 40, qPrintable(QString("sideband rejection %1 dB").arg(ratioDb)));
}

void TestDemod::lsbPassesLowerOnly()
{
    const double fs = 78125, audio = 48000;
    Demodulator d;
    d.configure(fs, audio);
    DemodParams p;
    p.mode = Mode::LSB;
    p.tuneHz = -3000;
    p.bandwidthHz = 2700;
    p.agc = AgcMode::Off;
    p.volume = 1.0f;
    d.setParams(p);

    auto lower = tone(fs, p.tuneHz - 800, 0.0005f, size_t(fs * 1.5));
    auto upper = tone(fs, p.tuneHz + 800, 0.0005f, size_t(fs * 1.5));
    auto outL = run(d, lower, fs, audio);
    d.reset();
    auto outU = run(d, upper, fs, audio);

    auto rms = [](const std::vector<float>& v) {
        double s = 0;
        for (size_t i = v.size() / 2; i < v.size(); ++i) s += double(v[i]) * v[i];
        return std::sqrt(s / double(v.size() / 2));
    };
    QVERIFY2(std::abs(peakHz(outL, audio, 8192) - 800) < 12, "LSB tone frequency");
    const double ratioDb = 20 * std::log10(rms(outL) / std::max(1e-9, rms(outU)));
    QVERIFY2(ratioDb > 40, qPrintable(QString("sideband rejection %1 dB").arg(ratioDb)));
}

void TestDemod::filterEdges()
{
    DemodParams p;
    float lo, hi;
    p.mode = Mode::CW; p.bandwidthHz = 400;
    Demodulator::filterEdges(p, lo, hi);
    QCOMPARE(lo, -200.0f); QCOMPARE(hi, 200.0f);
    p.mode = Mode::USB; p.bandwidthHz = 2700;
    Demodulator::filterEdges(p, lo, hi);
    QCOMPARE(lo, 200.0f); QCOMPARE(hi, 2900.0f);
    p.mode = Mode::LSB;
    Demodulator::filterEdges(p, lo, hi);
    QCOMPARE(lo, -2900.0f); QCOMPARE(hi, -200.0f);
}

QTEST_GUILESS_MAIN(TestDemod)
#include "tst_demod.moc"
