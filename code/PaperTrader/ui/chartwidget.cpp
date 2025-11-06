#include "chartwidget.h"
#include <QPainter>
#include <QWheelEvent>
#include <QMouseEvent>
#include <algorithm>

Q_LOGGING_CATEGORY(lcChart, "chart")

ChartWidget::ChartWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(300);
    setMouseTracking(true);
}

void ChartWidget::appendCandle(const Candle &c)
{
    m_candles.append(c);
    refreshVisibleFromWidth();

    if (m_followTail) {
        const double rightEdge = static_cast<double>(total());
        m_viewStart = std::max(0.0, rightEdge - m_visibleCount);
    }

    clampView();
    if (isVisible()) update();
}

void ChartWidget::refreshVisibleFromWidth()
{
    int w = std::max(1, width());
    m_visibleCount = std::max(1.0, w / static_cast<double>(pitch()));
}

void ChartWidget::clampView()
{
    refreshVisibleFromWidth();
    double maxStart = std::max(0.0, static_cast<double>(total()) - m_visibleCount);
    m_viewStart = std::clamp(m_viewStart, 0.0, maxStart);
}

bool ChartWidget::latestVisible() const
{
    return (m_viewStart + m_visibleCount) >= (static_cast<double>(total()) - 0.5);
}

void ChartWidget::paintEvent(QPaintEvent *)
{
    if (m_candles.isEmpty()) return;
    QPainter p(this);
    p.fillRect(rect(), QColor("#101010"));

    refreshVisibleFromWidth();
    clampView();

    int totalCount = total();
    int startIdx = std::clamp(static_cast<int>(std::floor(m_viewStart)), 0, std::max(0, totalCount - 1));
    int endIdx = std::clamp(static_cast<int>(std::ceil(m_viewStart + m_visibleCount)),
                            startIdx + 1, totalCount);

    double minP = 1e12, maxP = -1e12;
    for (int i = startIdx; i < endIdx; ++i) {
        minP = std::min(minP, m_candles[i].low);
        maxP = std::max(maxP, m_candles[i].high);
    }

    drawCandles(p, startIdx, endIdx, minP, maxP);

    if (m_followTail && !m_panning) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#00ff66"));
        QRect badge(width() - 50, height() - 25, 40, 18);
        p.drawRoundedRect(badge, 6, 6);
        p.setPen(Qt::black);
        p.drawText(badge, Qt::AlignCenter, "LIVE");
    }
}

void ChartWidget::drawCandles(QPainter &p, int startIdx, int endIdx,
                              double minP, double maxP)
{
    int h = height();
    double priceRange = std::max(1.0, maxP - minP);
    double yScale = (h / priceRange) * m_verticalScale;
    double yOffset = m_verticalPan * h;
    int pxPitch = pitch();

    for (int i = startIdx; i < endIdx; ++i) {
        const Candle &c = m_candles[i];
        double rel = static_cast<double>(i) - m_viewStart;
        int x = static_cast<int>(rel * pxPitch);
        double yO = h - ((c.open  - minP) * yScale) - yOffset;
        double yC = h - ((c.close - minP) * yScale) - yOffset;
        double yH = h - ((c.high  - minP) * yScale) - yOffset;
        double yL = h - ((c.low   - minP) * yScale) - yOffset;
        QColor color = (c.close >= c.open) ? QColor("#00ff66") : QColor("#ff3355");
        p.setPen(color);
        p.drawLine(x + m_candleWidth / 2, static_cast<int>(yH),
                   x + m_candleWidth / 2, static_cast<int>(yL));
        p.setBrush(color);
        p.drawRect(x, static_cast<int>(std::min(yO, yC)),
                   m_candleWidth, static_cast<int>(std::abs(yC - yO)));
    }
}

void ChartWidget::wheelEvent(QWheelEvent *e)
{
    bool ctrl = e->modifiers() & Qt::ControlModifier;
    double steps = e->angleDelta().y() / 120.0;

    if (ctrl) {
        m_verticalScale *= (1.0 + steps * 0.1);
        m_verticalScale = std::clamp(m_verticalScale, 0.5, 3.0);
    } else {
        double oldVisible = m_visibleCount;
        m_scale *= (1.0 + steps * 0.1);
        m_scale = std::clamp(m_scale, 0.5, 4.0);
        m_candleWidth = std::max(3, static_cast<int>(6 * m_scale));

        refreshVisibleFromWidth();
        double oldRight = m_viewStart + oldVisible;
        m_viewStart = oldRight - m_visibleCount;

        if (m_followTail) {
            double rightEdge = static_cast<double>(total());
            m_viewStart = std::max(0.0, rightEdge - m_visibleCount);
        }
    }

    m_followTail = !m_panning && latestVisible();
    clampView();
    update();
}

void ChartWidget::mousePressEvent(QMouseEvent *e)
{
    m_lastMousePos = e->pos();
    m_panning = true;
    m_followTail = false;
}

void ChartWidget::mouseMoveEvent(QMouseEvent *e)
{
    if (!m_panning) return;
    int dx = e->pos().x() - m_lastMousePos.x();
    int dy = e->pos().y() - m_lastMousePos.y();
    double pxPerCandle = std::max(1.0, static_cast<double>(pitch()));
    m_viewStart -= dx / pxPerCandle;
    m_verticalPan -= dy / static_cast<double>(height());
    m_verticalPan = std::clamp(m_verticalPan, -1.0, 1.0);
    clampView();
    m_lastMousePos = e->pos();
    update();
}

void ChartWidget::mouseReleaseEvent(QMouseEvent *)
{
    m_panning = false;
    m_followTail = latestVisible();
}

void ChartWidget::clearCandles()
{
    m_candles.clear();
    m_viewStart = 0.0;
    m_followTail = true;
    update();
}
