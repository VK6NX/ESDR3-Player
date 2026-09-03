// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/PanoramaItem.h"

#include "ui/FftMapping.h"
#include "ui/Palettes.h"

#include <QFontMetricsF>
#include <QHoverEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QQuickWindow>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace esdr3 {

namespace {

constexpr double kLeftMargin = 58;
constexpr double kRightMargin = 6;
constexpr double kTopMargin = 4;
constexpr double kAxisHeight = 20;
constexpr double kMinSpanHz = 1000;

const QColor kBackground(0x0b, 0x12, 0x20);
const QColor kGrid(0x22, 0x30, 0x4a);
const QColor kAxisText(0x9f, 0xb0, 0xc0);
const QColor kSpectrumLine(0x6f, 0xd0, 0xf0);
const QColor kSpectrumFill(0x6f, 0xd0, 0xf0, 70);
const QColor kVfo(0xff, 0xcc, 0x33);
const QColor kFilter(0xff, 0xcc, 0x33, 50);
const QColor kCursor(0xff, 0x60, 0x60);
const QColor kStamp(0xe0, 0xe6, 0xee);

QString formatKhz(double hz, int decimals)
{
    return QString::number(hz / 1000.0, 'f', decimals);
}

}

PanoramaItem::PanoramaItem(QQuickItem* parent) : QQuickPaintedItem(parent)
{
    setRenderTarget(QQuickPaintedItem::FramebufferObject);
    setAcceptedMouseButtons(Qt::LeftButton);
    setAcceptHoverEvents(true);
    setFlag(ItemHasContents, true);
    setAntialiasing(true);
}

PanoramaItem::~PanoramaItem() = default;

void PanoramaItem::setSource(QObject* src)
{
    if (m_source == src) return;
    if (m_source) disconnect(m_source, nullptr, this, nullptr);
    m_source = src;
    if (m_source) {
        connect(m_source, SIGNAL(psdReady(esdr3::PsdFrame)), this, SLOT(setPsd(esdr3::PsdFrame)),
                Qt::QueuedConnection);
    }
    emit sourceChanged();
}

void PanoramaItem::notifyView()
{
    remapSpectrum();
    emit viewChanged();
    update();
}

void PanoramaItem::setCenterHz(double hz)
{
    if (m_centerHz == hz) return;
    const bool keepView = m_sampleRate > 0 && m_spanHz > 0;
    const double offset = m_viewCenterHz - m_centerHz;
    m_centerHz = hz;
    m_viewCenterHz = keepView ? hz + offset : hz;
    if (m_vfoHz == 0 && hz > 0) {
        m_vfoHz = hz;
        emit vfoChanged();
    }
    clampView();
    notifyView();
}

void PanoramaItem::setSampleRate(double fs)
{
    if (m_sampleRate == fs) return;
    m_sampleRate = fs;
    if (m_spanHz <= 0 || m_spanHz > fs) m_spanHz = fs;
    if (m_viewCenterHz == 0) m_viewCenterHz = m_centerHz;
    clampView();
    notifyView();
}

void PanoramaItem::setSpanHz(double hz)
{
    if (m_sampleRate <= 0) return;
    hz = std::clamp(hz, kMinSpanHz, m_sampleRate);
    if (m_spanHz == hz) return;
    m_spanHz = hz;
    clampView();
    notifyView();
}

void PanoramaItem::setViewCenterHz(double hz)
{
    if (m_viewCenterHz == hz) return;
    m_viewCenterHz = hz;
    clampView();
    notifyView();
}

void PanoramaItem::setVfoHz(double hz)
{
    if (m_vfoHz == hz) return;
    m_vfoHz = hz;
    emit vfoChanged();
    update();
}

void PanoramaItem::setFilterLowHz(double hz)
{
    if (m_filterLow == hz) return;
    m_filterLow = hz;
    emit filterChanged();
    update();
}

void PanoramaItem::setFilterHighHz(double hz)
{
    if (m_filterHigh == hz) return;
    m_filterHigh = hz;
    emit filterChanged();
    update();
}

void PanoramaItem::setFilterVisible(bool on)
{
    if (m_filterVisible == on) return;
    m_filterVisible = on;
    emit filterChanged();
    update();
}

void PanoramaItem::setDbMin(double v)
{
    if (m_dbMin == v) return;
    m_dbMin = v;
    emit rangeChanged();
    update();
}

void PanoramaItem::setDbMax(double v)
{
    if (m_dbMax == v) return;
    m_dbMax = v;
    emit rangeChanged();
    update();
}

void PanoramaItem::setWfDbMin(double v)
{
    if (m_wfDbMin == v) return;
    m_wfDbMin = v;
    emit rangeChanged();
}

void PanoramaItem::setWfDbMax(double v)
{
    if (m_wfDbMax == v) return;
    m_wfDbMax = v;
    emit rangeChanged();
}

void PanoramaItem::setWfAuto(bool on)
{
    if (m_wfAuto == on) return;
    m_wfAuto = on;
    m_autoValid = false;
    emit rangeChanged();
}

void PanoramaItem::setWaterfallRatio(double r)
{
    r = std::clamp(r, 0.1, 0.9);
    if (m_wfRatio == r) return;
    m_wfRatio = r;
    updateLayout();
    emit layoutChanged();
    update();
}

void PanoramaItem::setPaletteName(const QString& name)
{
    if (m_paletteName == name) return;
    m_paletteName = name;
    emit paletteChanged();
    update();
}

QStringList PanoramaItem::paletteNames() const { return esdr3::paletteNames(); }

double PanoramaItem::rbwHz() const
{
    return (m_frame.fftSize > 0 && m_sampleRate > 0) ? m_sampleRate / m_frame.fftSize : 0;
}

void PanoramaItem::setRecStart(const QDateTime& t)
{
    if (m_recStart == t) return;
    m_recStart = t;
    emit recStartChanged();
}

void PanoramaItem::resetZoom()
{
    if (m_sampleRate <= 0) return;
    m_spanHz = m_sampleRate;
    m_viewCenterHz = m_centerHz;
    clampView();
    notifyView();
}

void PanoramaItem::zoomIn() { zoomAt(1.0 / 1.5, m_plotRect.center().x()); }
void PanoramaItem::zoomOut() { zoomAt(1.5, m_plotRect.center().x()); }

void PanoramaItem::clearWaterfall()
{
    if (!m_waterfall.isNull()) m_waterfall.fill(kBackground);
    m_stamps.clear();
    m_haveStamp = false;
    update();
}

double PanoramaItem::devicePixelRatioSafe() const
{
    return window() ? window()->effectiveDevicePixelRatio() : 1.0;
}

void PanoramaItem::updateLayout()
{
    const double w = width(), h = height();
    const double plotW = std::max(1.0, w - kLeftMargin - kRightMargin);
    const double usable = std::max(1.0, h - kTopMargin - kAxisHeight);
    const double specH = std::max(20.0, std::floor(usable * (1.0 - m_wfRatio)));
    m_plotRect = QRectF(kLeftMargin, kTopMargin, plotW, specH);
    m_axisRect = QRectF(kLeftMargin, m_plotRect.bottom(), plotW, kAxisHeight);
    m_wfRect = QRectF(kLeftMargin, m_axisRect.bottom(), plotW, std::max(1.0, h - m_axisRect.bottom()));
    ensureWaterfallImage();
    remapSpectrum();
}

void PanoramaItem::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
    updateLayout();
}

void PanoramaItem::clampView()
{
    if (m_sampleRate <= 0) return;
    m_spanHz = std::clamp(m_spanHz, kMinSpanHz, m_sampleRate);
    const double lo = m_centerHz - m_sampleRate / 2 + m_spanHz / 2;
    const double hi = m_centerHz + m_sampleRate / 2 - m_spanHz / 2;
    m_viewCenterHz = std::clamp(m_viewCenterHz, lo, hi);
}

double PanoramaItem::freqAtX(double x) const
{
    if (m_plotRect.width() <= 0) return m_viewCenterHz;
    return viewStartHz() + (x - m_plotRect.left()) / m_plotRect.width() * m_spanHz;
}

double PanoramaItem::xAtFreq(double hz) const
{
    if (m_spanHz <= 0) return m_plotRect.left();
    return m_plotRect.left() + (hz - viewStartHz()) / m_spanHz * m_plotRect.width();
}

void PanoramaItem::zoomAt(double factor, double anchorX)
{
    if (m_sampleRate <= 0) return;
    const double fAnchor = freqAtX(anchorX);
    const double newSpan = std::clamp(m_spanHz * factor, kMinSpanHz, m_sampleRate);
    const double frac = m_plotRect.width() > 0 ? (anchorX - m_plotRect.left()) / m_plotRect.width() : 0.5;
    m_spanHz = newSpan;
    m_viewCenterHz = fAnchor - frac * newSpan + newSpan / 2;
    clampView();
    notifyView();
}

void PanoramaItem::setPsd(const PsdFrame& frame)
{
    if (!frame.dB || frame.fftSize <= 0) return;
    m_frame = frame;
    if (m_sampleRate != frame.sampleRate && frame.sampleRate > 0) setSampleRate(frame.sampleRate);
    remapSpectrum();
    appendWaterfallRow();
    emit frameChanged();
    update();
}

void PanoramaItem::remapSpectrum()
{
    const int w = int(std::lround(m_plotRect.width()));
    if (w <= 0) return;
    m_specMax.assign(size_t(w), float(m_dbMin));
    m_specMin.assign(size_t(w), float(m_dbMin));
    if (!m_frame.dB || m_sampleRate <= 0) return;
    const double startRel = viewStartHz() - m_centerHz;
    mapFftToPixels(m_frame.dB->data(), m_frame.fftSize, m_sampleRate, startRel, startRel + m_spanHz, w,
                   m_specMax.data(), m_specMin.data(), float(m_dbMin - 10));
}

void PanoramaItem::ensureWaterfallImage()
{
    const double dpr = devicePixelRatioSafe();
    const int w = std::max(1, int(std::lround(m_wfRect.width() * dpr)));
    const int h = std::max(1, int(std::lround(m_wfRect.height() * dpr)));
    if (m_waterfall.width() == w && m_waterfall.height() == h) return;
    if (m_waterfall.isNull()) {
        m_waterfall = QImage(w, h, QImage::Format_RGB32);
        m_waterfall.fill(kBackground);
    } else {
        m_waterfall = m_waterfall.scaled(w, h, Qt::IgnoreAspectRatio, Qt::FastTransformation);
        if (m_waterfall.format() != QImage::Format_RGB32)
            m_waterfall = m_waterfall.convertToFormat(QImage::Format_RGB32);
    }
    for (auto& s : m_stamps) s.row = std::min(s.row, h - 1);
}

void PanoramaItem::appendWaterfallRow()
{
    ensureWaterfallImage();
    const int w = m_waterfall.width();
    const int h = m_waterfall.height();
    if (w <= 0 || h <= 0 || !m_frame.dB || m_sampleRate <= 0) return;

    const float outside = -1000.0f;
    m_rowMax.assign(size_t(w), outside);
    const double startRel = viewStartHz() - m_centerHz;
    mapFftToPixels(m_frame.dB->data(), m_frame.fftSize, m_sampleRate, startRel, startRel + m_spanHz, w,
                   m_rowMax.data(), nullptr, outside);

    double lo = m_wfDbMin, hi = m_wfDbMax;
    if (m_wfAuto) {
        m_rowTmp.clear();
        for (float v : m_rowMax) if (v > outside) m_rowTmp.push_back(v);
        if (m_rowTmp.size() >= 16) {
            const size_t i25 = m_rowTmp.size() / 4;
            const size_t i995 = std::min(m_rowTmp.size() - 1, size_t(m_rowTmp.size() * 0.995));
            std::nth_element(m_rowTmp.begin(), m_rowTmp.begin() + qsizetype(i25), m_rowTmp.end());
            const double p25 = m_rowTmp[i25];
            std::nth_element(m_rowTmp.begin() + qsizetype(i25), m_rowTmp.begin() + qsizetype(i995), m_rowTmp.end());
            const double p995 = m_rowTmp[i995];
            double floorNow = p25 - 3.0;
            double ceilNow = std::clamp(p995 + 3.0, floorNow + 30.0, floorNow + 80.0);
            if (!m_autoValid) { m_autoFloor = floorNow; m_autoCeil = ceilNow; m_autoValid = true; }
            else {
                m_autoFloor += 0.05 * (floorNow - m_autoFloor);
                m_autoCeil += 0.05 * (ceilNow - m_autoCeil);
            }
        }
        if (m_autoValid) { lo = m_autoFloor; hi = m_autoCeil; }
    }

    const qsizetype bpl = m_waterfall.bytesPerLine();
    uchar* bits = m_waterfall.bits();
    std::memmove(bits + bpl, bits, size_t(bpl) * size_t(h - 1));

    const Palette& pal = paletteByName(m_paletteName);
    const double range = std::max(1.0, hi - lo);
    QRgb* row = reinterpret_cast<QRgb*>(bits);
    const QRgb bg = kBackground.rgb();
    for (int x = 0; x < w; ++x) {
        const float v = m_rowMax[size_t(x)];
        if (v <= outside) { row[x] = bg; continue; }
        int idx = int((v - lo) / range * 255.0);
        idx = std::clamp(idx, 0, 255);
        row[x] = pal.table[size_t(idx)];
    }

    for (auto& s : m_stamps) ++s.row;
    while (!m_stamps.empty() && m_stamps.back().row >= h) m_stamps.pop_back();

    const uint64_t stampEvery = uint64_t(m_sampleRate * 10.0);
    if (!m_haveStamp || m_frame.streamSample < m_lastStampSample ||
        m_frame.streamSample - m_lastStampSample >= stampEvery) {
        m_haveStamp = true;
        m_lastStampSample = m_frame.streamSample;
        QString text;
        const double secs = double(m_frame.streamSample) / m_sampleRate;
        if (m_recStart.isValid())
            text = m_recStart.addMSecs(qint64(secs * 1000)).toString(QStringLiteral("HH:mm:ss"));
        else {
            const int s = int(secs);
            text = QStringLiteral("%1:%2").arg(s / 60, 2, 10, QChar('0')).arg(s % 60, 2, 10, QChar('0'));
        }
        m_stamps.push_front({0, text});
    }
}

void PanoramaItem::setCursor(double x, bool inside)
{
    m_cursorX = x;
    m_cursorInside = inside && x >= m_plotRect.left() && x <= m_plotRect.right();
    m_cursorHz = m_cursorInside ? freqAtX(x) : 0;
    emit cursorChanged();
    update();
}

PanoramaItem::Drag PanoramaItem::hitTest(double x) const
{
    constexpr double grab = 5.0;
    if (m_spanHz <= 0 || m_vfoHz <= 0) return Drag::Pan;
    if (m_filterVisible) {
        if (std::abs(x - xAtFreq(m_vfoHz + m_filterLow)) <= grab) return Drag::FilterLow;
        if (std::abs(x - xAtFreq(m_vfoHz + m_filterHigh)) <= grab) return Drag::FilterHigh;
    }
    if (std::abs(x - xAtFreq(m_vfoHz)) <= grab) return Drag::Vfo;
    return Drag::Pan;
}

void PanoramaItem::updateCursorShape(double x)
{
    switch (hitTest(x)) {
    case Drag::FilterLow:
    case Drag::FilterHigh: QQuickItem::setCursor(Qt::SplitHCursor); break;
    case Drag::Vfo: QQuickItem::setCursor(Qt::SizeHorCursor); break;
    default: QQuickItem::unsetCursor(); break;
    }
}

void PanoramaItem::mousePressEvent(QMouseEvent* event)
{
    m_pressed = true;
    m_dragged = false;
    m_pressX = event->position().x();
    m_pressViewCenter = m_viewCenterHz;
    m_drag = hitTest(m_pressX);
    event->accept();
}

void PanoramaItem::mouseMoveEvent(QMouseEvent* event)
{
    const double x = event->position().x();
    if (m_pressed) {
        const double dx = x - m_pressX;
        if (!m_dragged && std::abs(dx) > 3) m_dragged = true;
        if (m_dragged && m_plotRect.width() > 0) {
            const double step = (event->modifiers() & Qt::ShiftModifier) ? 1.0 : 10.0;
            switch (m_drag) {
            case Drag::Pan:
                m_viewCenterHz = m_pressViewCenter - dx / m_plotRect.width() * m_spanHz;
                clampView();
                notifyView();
                break;
            case Drag::Vfo: {
                const double hz = std::round(freqAtX(x) / step) * step;
                setVfoHz(hz);
                emit vfoRequested(hz);
                break;
            }
            case Drag::FilterLow: {
                const double low = std::min(freqAtX(x) - m_vfoHz, m_filterHigh - 50.0);
                emit filterEdgeDragged(std::round(low / 10) * 10, m_filterHigh);
                break;
            }
            case Drag::FilterHigh: {
                const double high = std::max(freqAtX(x) - m_vfoHz, m_filterLow + 50.0);
                emit filterEdgeDragged(m_filterLow, std::round(high / 10) * 10);
                break;
            }
            case Drag::None: break;
            }
        }
    } else {
        updateCursorShape(x);
    }
    setCursor(x, true);
    event->accept();
}

void PanoramaItem::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_pressed && !m_dragged && m_drag != Drag::FilterLow && m_drag != Drag::FilterHigh) {
        const double step = (event->modifiers() & Qt::ShiftModifier) ? 1.0 : 10.0;
        const double hz = std::round(freqAtX(event->position().x()) / step) * step;
        setVfoHz(hz);
        emit vfoRequested(hz);
    }
    m_pressed = false;
    m_dragged = false;
    m_drag = Drag::None;
    updateCursorShape(event->position().x());
    event->accept();
}

void PanoramaItem::mouseDoubleClickEvent(QMouseEvent* event)
{
    resetZoom();
    event->accept();
}

void PanoramaItem::wheelEvent(QWheelEvent* event)
{
    const int dy = event->angleDelta().y();
    if (dy == 0) return;
    const double factor = std::pow(1.2, -dy / 120.0);
    zoomAt(factor, event->position().x());
    event->accept();
}

void PanoramaItem::hoverMoveEvent(QHoverEvent* event)
{
    updateCursorShape(event->position().x());
    setCursor(event->position().x(), true);
}

void PanoramaItem::hoverLeaveEvent(QHoverEvent*)
{
    QQuickItem::unsetCursor();
    setCursor(-1, false);
}

void PanoramaItem::paint(QPainter* p)
{
    p->fillRect(boundingRect(), kBackground);
    if (m_plotRect.width() <= 0) return;
    drawSpectrum(p);
    drawFrequencyAxis(p);
    drawWaterfall(p);
    drawMarkers(p);
}

void PanoramaItem::drawSpectrum(QPainter* p)
{
    const QRectF r = m_plotRect;
    const double range = std::max(1.0, m_dbMax - m_dbMin);
    auto yOf = [&](double db) { return r.bottom() - (db - m_dbMin) / range * r.height(); };

    p->setRenderHint(QPainter::Antialiasing, false);
    QFont f = p->font();
    f.setPointSizeF(10);
    p->setFont(f);

    const int step = range <= 120 ? 10 : 20;
    const int first = int(std::ceil(m_dbMin / step)) * step;
    p->setPen(kGrid);
    for (int db = first; db <= int(m_dbMax); db += step) {
        const double y = std::round(yOf(db)) + 0.5;
        p->drawLine(QPointF(r.left(), y), QPointF(r.right(), y));
    }
    p->setPen(kAxisText);
    for (int db = first; db <= int(m_dbMax); db += step) {
        const double y = yOf(db);
        if (y < r.top() + 6 || y > r.bottom() - 2) continue;
        p->drawText(QRectF(0, y - 8, kLeftMargin - 4, 16), Qt::AlignRight | Qt::AlignVCenter, QString::number(db));
    }

    const int w = int(m_specMax.size());
    if (w <= 0 || !m_frame.dB) return;

    QPainterPath line;
    QPainterPath fill;
    bool started = false;
    for (int x = 0; x < w; ++x) {
        const double v = std::clamp(double(m_specMax[size_t(x)]), m_dbMin - 5, m_dbMax + 5);
        const double y = yOf(v);
        const double px = r.left() + x + 0.5;
        if (!started) {
            line.moveTo(px, y);
            fill.moveTo(px, r.bottom());
            fill.lineTo(px, y);
            started = true;
        } else {
            line.lineTo(px, y);
            fill.lineTo(px, y);
        }
    }
    fill.lineTo(r.left() + w - 0.5, r.bottom());
    fill.closeSubpath();

    p->save();
    p->setClipRect(r);
    p->setRenderHint(QPainter::Antialiasing, true);
    p->fillPath(fill, kSpectrumFill);
    p->setPen(QPen(kSpectrumLine, 1.0));
    p->drawPath(line);
    p->restore();
}

void PanoramaItem::drawFrequencyAxis(QPainter* p)
{
    const QRectF r = m_axisRect;
    p->fillRect(r, kBackground.lighter(130));
    if (m_spanHz <= 0 || r.width() <= 0) return;

    static const double candidates[] = {10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000, 100000, 200000};
    const double pxPerHz = r.width() / m_spanHz;
    double stepHz = candidates[std::size(candidates) - 1];
    for (double c : candidates)
        if (c * pxPerHz >= 80) { stepHz = c; break; }
    const int decimals = stepHz >= 1000 ? 0 : (stepHz >= 100 ? 1 : 2);

    p->setRenderHint(QPainter::Antialiasing, false);
    QFont f = p->font();
    f.setPointSizeF(10);
    p->setFont(f);

    const double start = viewStartHz();
    const double end = start + m_spanHz;
    double tick = std::ceil(start / stepHz) * stepHz;
    for (; tick <= end; tick += stepHz) {
        const double x = std::round(xAtFreq(tick)) + 0.5;
        p->setPen(kGrid);
        p->drawLine(QPointF(x, m_plotRect.top()), QPointF(x, m_plotRect.bottom()));
        p->setPen(kAxisText);
        p->drawLine(QPointF(x, r.top()), QPointF(x, r.top() + 4));
        p->drawText(QRectF(x - 60, r.top() + 3, 120, r.height() - 3), Qt::AlignHCenter | Qt::AlignVCenter,
                    formatKhz(tick, decimals));
    }
    p->setPen(kAxisText);
    p->drawText(QRectF(r.right() - 34, r.top() + 3, 34, r.height() - 3), Qt::AlignRight | Qt::AlignVCenter,
                QStringLiteral("kHz"));
}

void PanoramaItem::drawWaterfall(QPainter* p)
{
    if (m_waterfall.isNull()) return;
    p->setRenderHint(QPainter::SmoothPixmapTransform, false);
    p->drawImage(m_wfRect, m_waterfall);

    const double dpr = devicePixelRatioSafe();
    QFont f = p->font();
    f.setPointSizeF(9);
    p->setFont(f);
    for (const auto& s : m_stamps) {
        const double y = m_wfRect.top() + s.row / dpr;
        if (y > m_wfRect.bottom() - 2) continue;
        p->setPen(QColor(0, 0, 0, 150));
        p->drawLine(QPointF(m_wfRect.left(), y + 0.5), QPointF(m_wfRect.left() + 6, y + 0.5));
        p->setPen(kStamp);
        p->drawText(QRectF(0, y - 7, kLeftMargin - 3, 14), Qt::AlignRight | Qt::AlignVCenter, s.text);
    }
}

void PanoramaItem::drawMarkers(QPainter* p)
{
    p->setRenderHint(QPainter::Antialiasing, false);
    const double top = m_plotRect.top();
    const double bottom = m_wfRect.bottom();

    if (m_filterVisible && m_spanHz > 0) {
        const double x0 = xAtFreq(m_vfoHz + m_filterLow);
        const double x1 = xAtFreq(m_vfoHz + m_filterHigh);
        QRectF box(std::min(x0, x1), top, std::abs(x1 - x0), m_plotRect.height());
        box = box.intersected(m_plotRect);
        if (box.width() > 0) {
            p->fillRect(box, kFilter);
            p->setPen(QPen(QColor(kVfo.red(), kVfo.green(), kVfo.blue(), 140), 1.0));
            p->drawLine(QPointF(std::round(box.left()) + 0.5, top), QPointF(std::round(box.left()) + 0.5, m_plotRect.bottom()));
            p->drawLine(QPointF(std::round(box.right()) - 0.5, top), QPointF(std::round(box.right()) - 0.5, m_plotRect.bottom()));
        }
    }

    if (m_vfoHz > 0) {
        const double x = std::round(xAtFreq(m_vfoHz)) + 0.5;
        if (x >= m_plotRect.left() && x <= m_plotRect.right()) {
            p->setPen(QPen(kVfo, 1.0));
            p->drawLine(QPointF(x, top), QPointF(x, bottom));
        }
    }

    if (m_cursorInside) {
        const double x = std::round(m_cursorX) + 0.5;
        QPen pen(kCursor, 1.0, Qt::DashLine);
        p->setPen(pen);
        p->drawLine(QPointF(x, top), QPointF(x, bottom));
        const QString label = formatKhz(m_cursorHz, 3) + QStringLiteral(" kHz");
        QFont f = p->font();
        f.setPointSizeF(10);
        p->setFont(f);
        const QFontMetricsF fm(f);
        const double tw = fm.horizontalAdvance(label) + 8;
        double lx = x + 6;
        if (lx + tw > m_plotRect.right()) lx = x - 6 - tw;
        const QRectF bg(lx, top + 4, tw, fm.height() + 4);
        p->fillRect(bg, QColor(0, 0, 0, 170));
        p->setPen(kCursor);
        p->drawText(bg, Qt::AlignCenter, label);
    }
}

}
