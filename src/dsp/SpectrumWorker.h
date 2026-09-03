// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/TailBuffer.h"
#include "core/Types.h"
#include "dsp/PsdComputer.h"

#include <QObject>
#include <QTimer>
#include <memory>
#include <vector>

namespace esdr3 {

class SpectrumWorker : public QObject {
    Q_OBJECT
public:
    explicit SpectrumWorker(std::shared_ptr<IqTailBuffer> tail, QObject* parent = nullptr);
    ~SpectrumWorker() override;

public slots:
    void start();
    void stop();
    void setFftSize(int n);
    void setAveraging(float a);
    void setFrameRate(int fps);
    void setSampleRate(double fs);
    void setCenterHz(double hz);
    void reset();

signals:
    void psdReady(esdr3::PsdFrame frame);

private:
    void tick();
    void rebuild();

    std::shared_ptr<IqTailBuffer> m_tail;
    QTimer* m_timer = nullptr;
    std::unique_ptr<PsdComputer> m_psd;
    std::vector<std::complex<float>> m_in;
    std::vector<float> m_db;
    std::vector<float> m_avg;
    bool m_avgValid = false;
    int m_fftSize = 16384;
    float m_averaging = 0.5f;
    double m_sampleRate = 0;
    double m_centerHz = 0;
    uint64_t m_lastStreamPos = ~0ull;
};

}
