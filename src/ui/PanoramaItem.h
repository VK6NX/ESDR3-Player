// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/Types.h"

#include <QImage>
#include <QPointer>
#include <QQuickPaintedItem>
#include <QtQml/qqmlregistration.h>
#include <deque>
#include <vector>

namespace esdr3 {

class PanoramaItem : public QQuickPaintedItem {
    Q_OBJECT
    QML_NAMED_ELEMENT(PanoramaItem)

    Q_PROPERTY(QObject* source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(double centerHz READ centerHz WRITE setCenterHz NOTIFY viewChanged)
    Q_PROPERTY(double sampleRate READ sampleRate WRITE setSampleRate NOTIFY viewChanged)
    Q_PROPERTY(double spanHz READ spanHz WRITE setSpanHz NOTIFY viewChanged)
    Q_PROPERTY(double viewCenterHz READ viewCenterHz WRITE setViewCenterHz NOTIFY viewChanged)
    Q_PROPERTY(double vfoHz READ vfoHz WRITE setVfoHz NOTIFY vfoChanged)
    Q_PROPERTY(double filterLowHz READ filterLowHz WRITE setFilterLowHz NOTIFY filterChanged)
    Q_PROPERTY(double filterHighHz READ filterHighHz WRITE setFilterHighHz NOTIFY filterChanged)
    Q_PROPERTY(bool filterVisible READ filterVisible WRITE setFilterVisible NOTIFY filterChanged)
    Q_PROPERTY(double dbMin READ dbMin WRITE setDbMin NOTIFY rangeChanged)
    Q_PROPERTY(double dbMax READ dbMax WRITE setDbMax NOTIFY rangeChanged)
    Q_PROPERTY(double wfDbMin READ wfDbMin WRITE setWfDbMin NOTIFY rangeChanged)
    Q_PROPERTY(double wfDbMax READ wfDbMax WRITE setWfDbMax NOTIFY rangeChanged)
    Q_PROPERTY(bool wfAuto READ wfAuto WRITE setWfAuto NOTIFY rangeChanged)
    Q_PROPERTY(double wfAutoMin READ wfAutoMin NOTIFY frameChanged)
    Q_PROPERTY(double wfAutoMax READ wfAutoMax NOTIFY frameChanged)
    Q_PROPERTY(double waterfallRatio READ waterfallRatio WRITE setWaterfallRatio NOTIFY layoutChanged)
    Q_PROPERTY(QString paletteName READ paletteName WRITE setPaletteName NOTIFY paletteChanged)
    Q_PROPERTY(QStringList paletteNames READ paletteNames CONSTANT)
    Q_PROPERTY(double cursorHz READ cursorHz NOTIFY cursorChanged)
    Q_PROPERTY(bool cursorInside READ cursorInside NOTIFY cursorChanged)
    Q_PROPERTY(double rbwHz READ rbwHz NOTIFY frameChanged)
    Q_PROPERTY(QDateTime recStart READ recStart WRITE setRecStart NOTIFY recStartChanged)

public:
    explicit PanoramaItem(QQuickItem* parent = nullptr);
    ~PanoramaItem() override;

    QObject* source() const { return m_source; }
    void setSource(QObject* src);

    double centerHz() const { return m_centerHz; }
    void setCenterHz(double hz);
    double sampleRate() const { return m_sampleRate; }
    void setSampleRate(double fs);
    double spanHz() const { return m_spanHz; }
    void setSpanHz(double hz);
    double viewCenterHz() const { return m_viewCenterHz; }
    void setViewCenterHz(double hz);
    double vfoHz() const { return m_vfoHz; }
    void setVfoHz(double hz);
    double filterLowHz() const { return m_filterLow; }
    void setFilterLowHz(double hz);
    double filterHighHz() const { return m_filterHigh; }
    void setFilterHighHz(double hz);
    bool filterVisible() const { return m_filterVisible; }
    void setFilterVisible(bool on);
    double dbMin() const { return m_dbMin; }
    void setDbMin(double v);
    double dbMax() const { return m_dbMax; }
    void setDbMax(double v);
    double wfDbMin() const { return m_wfDbMin; }
    void setWfDbMin(double v);
    double wfDbMax() const { return m_wfDbMax; }
    void setWfDbMax(double v);
    bool wfAuto() const { return m_wfAuto; }
    void setWfAuto(bool on);
    double wfAutoMin() const { return m_autoFloor; }
    double wfAutoMax() const { return m_autoCeil; }
    double waterfallRatio() const { return m_wfRatio; }
    void setWaterfallRatio(double r);
    QString paletteName() const { return m_paletteName; }
    void setPaletteName(const QString& name);
    QStringList paletteNames() const;
    double cursorHz() const { return m_cursorHz; }
    bool cursorInside() const { return m_cursorInside; }
    double rbwHz() const;
    QDateTime recStart() const { return m_recStart; }
    void setRecStart(const QDateTime& t);

    Q_INVOKABLE void resetZoom();
    Q_INVOKABLE void zoomIn();
    Q_INVOKABLE void zoomOut();
    Q_INVOKABLE void clearWaterfall();

    void paint(QPainter* painter) override;

public slots:
    void setPsd(const esdr3::PsdFrame& frame);

signals:
    void sourceChanged();
    void viewChanged();
    void vfoChanged();
    void vfoRequested(double hz);
    void filterEdgeDragged(double lowHz, double highHz);
    void filterChanged();
    void rangeChanged();
    void layoutChanged();
    void paletteChanged();
    void cursorChanged();
    void frameChanged();
    void recStartChanged();

protected:
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void hoverMoveEvent(QHoverEvent* event) override;
    void hoverLeaveEvent(QHoverEvent* event) override;

private:
    struct TimeStamp {
        int row;
        QString text;
    };
    enum class Drag { None, Pan, Vfo, FilterLow, FilterHigh };

    Drag hitTest(double x) const;
    void updateCursorShape(double x);

    void updateLayout();
    void clampView();
    void zoomAt(double factor, double anchorX);
    double freqAtX(double x) const;
    double xAtFreq(double hz) const;
    double viewStartHz() const { return m_viewCenterHz - m_spanHz / 2; }
    void remapSpectrum();
    void appendWaterfallRow();
    void ensureWaterfallImage();
    void setCursor(double x, bool inside);
    double devicePixelRatioSafe() const;
    void notifyView();

    void drawSpectrum(QPainter* p);
    void drawFrequencyAxis(QPainter* p);
    void drawWaterfall(QPainter* p);
    void drawMarkers(QPainter* p);

    QPointer<QObject> m_source;
    PsdFrame m_frame;

    double m_centerHz = 0;
    double m_sampleRate = 0;
    double m_spanHz = 0;
    double m_viewCenterHz = 0;
    double m_vfoHz = 0;
    double m_filterLow = -250;
    double m_filterHigh = 250;
    bool m_filterVisible = false;
    double m_dbMin = -140;
    double m_dbMax = -50;
    double m_wfDbMin = -130;
    double m_wfDbMax = -70;
    bool m_wfAuto = true;
    double m_autoFloor = 0;
    double m_autoCeil = 0;
    bool m_autoValid = false;
    std::vector<float> m_rowTmp;
    double m_wfRatio = 0.6;
    QString m_paletteName = QStringLiteral("Classic");
    QDateTime m_recStart;

    QRectF m_plotRect;
    QRectF m_axisRect;
    QRectF m_wfRect;

    std::vector<float> m_specMax;
    std::vector<float> m_specMin;
    std::vector<float> m_rowMax;
    QImage m_waterfall;
    std::deque<TimeStamp> m_stamps;
    uint64_t m_lastStampSample = 0;
    bool m_haveStamp = false;

    double m_cursorHz = 0;
    double m_cursorX = -1;
    bool m_cursorInside = false;

    bool m_pressed = false;
    bool m_dragged = false;
    Drag m_drag = Drag::None;
    double m_pressX = 0;
    double m_pressViewCenter = 0;
};

}
