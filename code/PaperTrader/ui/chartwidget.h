#pragma once
#include <QWidget>
#include <QVector>
#include <QLoggingCategory>
#include "core/models/candle.h"

Q_DECLARE_LOGGING_CATEGORY(lcChart)

class ChartWidget : public QWidget {
    Q_OBJECT
public:
    explicit ChartWidget(QWidget *parent = nullptr);
    void appendCandle(const Candle &c);
    void clearCandles();


protected:
    void paintEvent(QPaintEvent *) override;
    void wheelEvent(QWheelEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;

private:
    QVector<Candle> m_candles;
    double m_scale = 1.0;
    int m_candleWidth = 6;
    int m_spacing = 2;
    double m_viewStart = 0.0;
    double m_visibleCount = 0.0;
    double m_verticalScale = 1.0;
    double m_verticalPan = 0.0;
    QPoint m_lastMousePos;
    bool m_panning = false;
    bool m_followTail = true;

    int total() const { return static_cast<int>(m_candles.size()); }
    int pitch() const { return std::max(1, m_candleWidth + m_spacing); }
    void refreshVisibleFromWidth();
    void clampView();
    bool latestVisible() const;
    void drawCandles(QPainter &p, int startIdx, int endIdx,
                     double minPrice, double maxPrice);
};
