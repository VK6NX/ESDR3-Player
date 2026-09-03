// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/WavReader.h"
#include "dsp/PsdComputer.h"

#include <QBuffer>
#include <QFile>
#include <QTemporaryFile>
#include <QtTest>
#include <cmath>

using namespace esdr3;

namespace {

QByteArray readData(const QString& name)
{
    QFile f(QStringLiteral(TEST_DATA_DIR "/") + name);
    if (!f.open(QIODevice::ReadOnly)) return {};
    return f.readAll();
}

QByteArray makeWav24(const QList<QPair<int32_t, int32_t>>& samples, uint32_t rate)
{
    QByteArray b;
    auto put16 = [&](uint16_t v) { b.append(char(v & 0xff)); b.append(char(v >> 8)); };
    auto put32 = [&](uint32_t v) { for (int i = 0; i < 4; ++i) b.append(char((v >> (8 * i)) & 0xff)); };
    b.append("RIFF"); put32(8); b.append("WAVE");
    b.append("fmt "); put32(16); put16(1); put16(2); put32(rate); put32(rate * 6); put16(6); put16(24);
    b.append("data"); put32(0xFFFFFFFFu);
    for (auto [i, q] : samples) {
        for (int32_t v : {i, q}) {
            b.append(char(v & 0xff)); b.append(char((v >> 8) & 0xff)); b.append(char((v >> 16) & 0xff));
        }
    }
    return b;
}

}

class TestWav : public QObject {
    Q_OBJECT
private slots:
    void header39062();
    void header78125();
    void fileNameMeta();
    void read24bit();
    void lengthFromFileNotHeader();
    void psdTone();
};

void TestWav::header39062()
{
    QByteArray head = readData("esdr3_39062_head.bin");
    QVERIFY2(!head.isEmpty(), "tests/data/esdr3_39062_head.bin missing");
    QBuffer buf(&head);
    QVERIFY(buf.open(QIODevice::ReadOnly));

    WavFormat fmt; Esdr3Meta meta; QString err;
    QVERIFY2(WavReader::parseHeader(buf, fmt, meta, &err), qPrintable(err));
    QCOMPARE(fmt.formatTag, uint16_t(1));
    QCOMPARE(fmt.channels, uint16_t(2));
    QCOMPARE(fmt.bits, uint16_t(24));
    QCOMPARE(fmt.blockAlign, uint16_t(6));
    QCOMPARE(fmt.fmtRate, uint32_t(39062));
    QCOMPARE(fmt.dataOffset, uint64_t(116));
    QVERIFY(meta.present);
    QCOMPARE(meta.version, uint32_t(2));
    QCOMPARE(meta.sampleRate, 39062.5);
    QCOMPARE(meta.ddsHz, 14019200.0);
    QCOMPARE(meta.start.date(), QDate(2026, 8, 23));
    QCOMPARE(meta.start.time().toString("hh:mm:ss"), QString("09:20:02"));
    QVERIFY(meta.stop.isValid());
    QVERIFY(meta.stop > meta.start);
}

void TestWav::header78125()
{
    QByteArray head = readData("esdr3_78125_head.bin");
    QVERIFY2(!head.isEmpty(), "tests/data/esdr3_78125_head.bin missing");
    QBuffer buf(&head);
    QVERIFY(buf.open(QIODevice::ReadOnly));

    WavFormat fmt; Esdr3Meta meta; QString err;
    QVERIFY2(WavReader::parseHeader(buf, fmt, meta, &err), qPrintable(err));
    QCOMPARE(fmt.fmtRate, uint32_t(78125));
    QCOMPARE(meta.sampleRate, 78125.0);
    QCOMPARE(meta.ddsHz, 14025000.0);
    QCOMPARE(meta.start.date(), QDate(2026, 7, 28));
}

void TestWav::fileNameMeta()
{
    double dds = 0; QDateTime start;
    QVERIFY(WavReader::metaFromFileName(
        "/x/ExpertSDR3_IQ_Freq_14019200_Hz_Date_23-08-2026_Time_09-20-02.wav", &dds, &start));
    QCOMPARE(dds, 14019200.0);
    QCOMPARE(start.date(), QDate(2026, 8, 23));
    QCOMPARE(start.time(), QTime(9, 20, 2));
    QVERIFY(!WavReader::metaFromFileName("/x/other.wav", &dds, &start));
}

void TestWav::read24bit()
{
    QList<QPair<int32_t, int32_t>> s = {
        {0, 0}, {8388607, -8388608}, {-1, 1}, {4194304, -4194304}, {123456, -654321}};
    QTemporaryFile f;
    QVERIFY(f.open());
    f.write(makeWav24(s, 48000));
    f.close();

    WavReader r; QString err;
    QVERIFY2(r.open(f.fileName(), &err), qPrintable(err));
    QCOMPARE(r.totalSamples(), uint64_t(5));
    QCOMPARE(r.sampleRate(), 48000.0);
    QCOMPARE(r.ddsHz(), 0.0);

    std::complex<float> out[8];
    QCOMPARE(r.read(out, 8), size_t(5));
    QCOMPARE(out[0], std::complex<float>(0, 0));
    QCOMPARE(out[1].real(), 8388607.0f / 8388608.0f);
    QCOMPARE(out[1].imag(), -1.0f);
    QCOMPARE(out[2].real(), -1.0f / 8388608.0f);
    QCOMPARE(out[2].imag(), 1.0f / 8388608.0f);
    QCOMPARE(out[3].real(), 0.5f);
    QCOMPARE(out[3].imag(), -0.5f);
    QCOMPARE(out[4].real(), 123456.0f / 8388608.0f);
    QCOMPARE(out[4].imag(), -654321.0f / 8388608.0f);
    QCOMPARE(r.read(out, 8), size_t(0));

    QVERIFY(r.seek(3));
    QCOMPARE(r.position(), uint64_t(3));
    QCOMPARE(r.read(out, 8), size_t(2));
    QCOMPARE(out[0].real(), 0.5f);
}

void TestWav::lengthFromFileNotHeader()
{
    QTemporaryFile f;
    QVERIFY(f.open());
    QByteArray b = makeWav24({{1, 2}, {3, 4}, {5, 6}}, 39062);
    b.append('\0');
    f.write(b);
    f.close();

    WavReader r;
    QVERIFY(r.open(f.fileName()));
    QCOMPARE(r.totalSamples(), uint64_t(3));

    QFile app(f.fileName());
    QVERIFY(app.open(QIODevice::Append));
    app.write(QByteArray(5, '\0'));
    app.close();
    QVERIFY(r.refreshLength());
    QCOMPARE(r.totalSamples(), uint64_t(4));
}

void TestWav::psdTone()
{
    const int n = 4096;
    const double fs = 78125.0;
    const double f0 = 12345.0;
    const size_t N = static_cast<size_t>(n);
    std::vector<std::complex<float>> x(N);
    for (int i = 0; i < n; ++i) {
        const double ph = 2 * M_PI * f0 * i / fs;
        x[size_t(i)] = std::complex<float>(float(std::cos(ph)), float(std::sin(ph)));
    }
    std::vector<float> db(N);
    PsdComputer psd(n);
    psd.compute(x.data(), db.data());

    int peak = int(std::max_element(db.begin(), db.end()) - db.begin());
    const double peakHz = (peak - n / 2) * fs / n;
    QVERIFY2(std::abs(peakHz - f0) < fs / n, qPrintable(QString::number(peakHz)));
    QVERIFY2(std::abs(db[size_t(peak)]) < 0.5, qPrintable(QString::number(db[size_t(peak)])));
    QVERIFY(db[size_t(peak + 200)] < -80.0f);
}

QTEST_GUILESS_MAIN(TestWav)
#include "tst_wav.moc"
