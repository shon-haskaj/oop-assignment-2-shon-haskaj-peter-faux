#include "chartwidget.h"

#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QWheelEvent>
#include <algorithm>
#include <limits>
#include <cmath>
#include <QtGlobal>

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
    const int w = std::max(1, chartRect().width());
    m_visibleCount = std::max(1.0, w / static_cast<double>(pitch()));
}

void ChartWidget::clampView()
{
    refreshVisibleFromWidth();
    const double maxStart = std::max(0.0, static_cast<double>(total()) - m_visibleCount);
    m_viewStart = std::clamp(m_viewStart, 0.0, maxStart);
}

bool ChartWidget::latestVisible() const
{
    return (m_viewStart + m_visibleCount) >= (static_cast<double>(total()) - 0.5);
}

void ChartWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);

    if (m_candles.isEmpty()) {
        p.fillRect(rect(), QColor("#111319"));
        return;
    }

    QLinearGradient grad(rect().topLeft(), rect().bottomLeft());
    grad.setColorAt(0.0, QColor("#10131b"));
    grad.setColorAt(1.0, QColor("#07090f"));
    p.fillRect(rect(), grad);

    refreshVisibleFromWidth();
    clampView();

    const QRect area = chartRect();
    const int totalCount = total();
    const int startIdx = std::clamp(static_cast<int>(std::floor(m_viewStart)), 0, std::max(0, totalCount - 1));
    const int endIdx = std::clamp(static_cast<int>(std::ceil(m_viewStart + m_visibleCount)),
                                  startIdx + 1, totalCount);

    double minP = std::numeric_limits<double>::max();
    double maxP = std::numeric_limits<double>::lowest();
    for (int i = startIdx; i < endIdx; ++i) {
        minP = std::min(minP, m_candles[i].low);
        maxP = std::max(maxP, m_candles[i].high);
    }
    if (qFuzzyCompare(minP, maxP)) {
        minP -= 1.0;
        maxP += 1.0;
    }

    const double priceRange = std::max(1e-6, maxP - minP);
    const double yScale = (area.height() / priceRange) * m_verticalScale;
    const double yOffset = m_verticalPan * area.height();

    drawGridAndAxes(p, area, startIdx, endIdx, minP, maxP, yScale, yOffset);
    drawCandles(p, area, startIdx, endIdx, minP, maxP, yScale, yOffset);

    if (m_followTail && !m_panning) {
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 255, 102));
        QRect badge(width() - 60, height() - 30, 48, 20);
        p.drawRoundedRect(badge, 6, 6);
        p.setPen(Qt::black);
        p.drawText(badge, Qt::AlignCenter, "LIVE");
    }
}

QRect ChartWidget::chartRect() const
{
    QRect area = rect();
    area.adjust(m_chartMargins.left(), m_chartMargins.top(),
                -m_chartMargins.right(), -m_chartMargins.bottom());
    if (area.width() <= 0 || area.height() <= 0) {
        area = QRect(m_chartMargins.left(), m_chartMargins.top(), 1, 1);
    }
    return area;
}

void ChartWidget::drawCandles(QPainter &p, const QRect &area,
                              int startIdx, int endIdx,
                              double minP, double maxP,
                              double yScale, double yOffset)
{
    Q_UNUSED(maxP);
    const int pxPitch = pitch();
    const int baseX = area.left();
    const int maxX = area.right();

    p.setRenderHint(QPainter::Antialiasing, false);

    for (int i = startIdx; i < endIdx; ++i) {
        const Candle &c = m_candles[i];
        const double rel = static_cast<double>(i) - m_viewStart;
        const int x = baseX + static_cast<int>(rel * pxPitch);
        if (x > maxX + m_candleWidth)
            continue;

        const double yO = priceToY(c.open,  minP, area, yScale, yOffset);
        const double yC = priceToY(c.close, minP, area, yScale, yOffset);
        const double yH = priceToY(c.high,  minP, area, yScale, yOffset);
        const double yL = priceToY(c.low,   minP, area, yScale, yOffset);

        const QColor color = (c.close >= c.open)
                ? QColor(0, 214, 143)
                : QColor(252, 79, 112);

        p.setPen(QPen(color, 1));
        const int midX = x + m_candleWidth / 2;
        p.drawLine(midX, static_cast<int>(yH), midX, static_cast<int>(yL));

        p.setBrush(color);
        const int bodyTop = static_cast<int>(std::min(yO, yC));
        int bodyHeight = static_cast<int>(std::fabs(yC - yO));
        if (bodyHeight < 1) bodyHeight = 1;
        QRect bodyRect(x, bodyTop, m_candleWidth, bodyHeight);
        bodyRect = bodyRect.intersected(area);
        p.drawRect(bodyRect);
    }
}

void ChartWidget::wheelEvent(QWheelEvent *e)
{
    const bool ctrl = e->modifiers() & Qt::ControlModifier;
    const double steps = e->angleDelta().y() / 120.0;

    if (ctrl) {
        m_verticalScale *= (1.0 + steps * 0.1);
        m_verticalScale = std::clamp(m_verticalScale, 0.5, 3.0);
    } else {
        const double oldVisible = m_visibleCount;
        m_scale *= (1.0 + steps * 0.1);
        m_scale = std::clamp(m_scale, 0.5, 4.0);
        m_candleWidth = std::max(3, static_cast<int>(6 * m_scale));

        refreshVisibleFromWidth();
        const double oldRight = m_viewStart + oldVisible;
        m_viewStart = oldRight - m_visibleCount;

        if (m_followTail) {
            const double rightEdge = static_cast<double>(total());
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
    const int dx = e->pos().x() - m_lastMousePos.x();
    const int dy = e->pos().y() - m_lastMousePos.y();
    const double pxPerCandle = std::max(1.0, static_cast<double>(pitch()));
    m_viewStart -= dx / pxPerCandle;
    m_verticalPan -= dy / static_cast<double>(std::max(1, chartRect().height()));
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
    m_verticalPan = 0.0;
    m_verticalScale = 1.0;
    update();
}

void ChartWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    refreshVisibleFromWidth();
    clampView();
}

double ChartWidget::priceToY(double price, double minPrice,
                             const QRect &area, double yScale, double yOffset) const
{
    return area.bottom() - ((price - minPrice) * yScale) - yOffset;
}

void ChartWidget::drawGridAndAxes(QPainter &p, const QRect &area,
                                  int startIdx, int endIdx,
                                  double minPrice, double maxPrice,
                                  double yScale, double yOffset)
{
    Q_UNUSED(maxPrice);
    p.save();
    p.setRenderHint(QPainter::Antialiasing, false);

    const double visibleMin = minPrice - (yOffset / yScale);
    const double visibleMax = minPrice + ((area.height() - yOffset) / yScale);
    const double priceSpan = visibleMax - visibleMin;

    const double priceStep = niceStep(priceSpan / 6.0);
    const double firstLevel = std::floor(visibleMin / priceStep) * priceStep;

    QPen gridPen(QColor(255, 255, 255, 30));
    gridPen.setStyle(Qt::DashLine);
    p.setPen(gridPen);

    QFont labelFont = font();
    labelFont.setPointSizeF(labelFont.pointSizeF() * 0.9);
    p.setFont(labelFont);

    for (double level = firstLevel; level <= visibleMax + priceStep; level += priceStep) {
        const double y = priceToY(level, minPrice, area, yScale, yOffset);
        if (y < area.top() - 1 || y > area.bottom() + 1)
            continue;
        p.drawLine(area.left(), static_cast<int>(y), area.right(), static_cast<int>(y));

        QRect labelRect(area.right() + 8, static_cast<int>(y) - 10,
                         m_chartMargins.right() - 12, 20);
        p.setPen(QColor(200, 210, 230));
        p.drawText(labelRect, Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(level, 'f', priceStep < 1.0 ? 4 : 2));
    }

    // chart border
    p.setPen(QColor(255, 255, 255, 40));
    p.drawRect(area);

    // time axis
    const int visibleCount = std::max(1, endIdx - startIdx);
    const int step = std::max(1, visibleCount / 6);
    const int pxPitch = pitch();
    const int baseX = area.left();

    for (int i = startIdx; i < endIdx; i += step) {
        const double rel = static_cast<double>(i) - m_viewStart;
        const int x = baseX + static_cast<int>(rel * pxPitch);
        if (x < area.left() || x > area.right())
            continue;

        p.setPen(QColor(255, 255, 255, 30));
        p.drawLine(x, area.top(), x, area.bottom());

        const Candle &c = m_candles[i];
        QString text;
        if (c.timestamp.isValid()) {
            text = c.timestamp.toLocalTime().toString("hh:mm:ss");
        }
        if (text.isEmpty()) {
            text = QString::number(i);
        }

        QRect textRect(x - 50, area.bottom() + 8, 100, m_chartMargins.bottom() - 16);
        p.setPen(QColor(200, 210, 230));
        p.drawText(textRect, Qt::AlignHCenter | Qt::AlignTop, text);
    }

    p.restore();
}

double ChartWidget::niceStep(double rawStep) const
{
    if (rawStep <= 0.0)
        return 1.0;

    const double exponent = std::floor(std::log10(rawStep));
    const double fraction = rawStep / std::pow(10.0, exponent);
    double niceFraction;
    if (fraction < 1.5) niceFraction = 1.0;
    else if (fraction < 3.0) niceFraction = 2.0;
    else if (fraction < 7.0) niceFraction = 5.0;
    else niceFraction = 10.0;
    return niceFraction * std::pow(10.0, exponent);
}
