#include "portfoliomanager.h"

PortfolioManager::PortfolioManager(QObject *parent)
    : QObject(parent) {}

double PortfolioManager::totalUnrealizedPnL() const
{
    double total = 0.0;
    for (const Position &pos : m_positions) {
        total += pos.unrealizedPnL;
    }
    return total;
}

void PortfolioManager::onCandle(const Candle &c)
{
    const QString symbol = c.symbol.toUpper();
    m_lastPrices[symbol] = c.close;
    updateUnrealizedFor(symbol);
    emit portfolioChanged(m_cash, totalUnrealizedPnL(), positions());
}

void PortfolioManager::applyFill(const Order &order)
{
    if (order.filledQuantity <= 0)
        return;

    const QString symbol = order.symbol.toUpper();
    Position pos = m_positions.value(symbol);
    pos.symbol = symbol;

    const bool isBuy = order.side.compare("BUY", Qt::CaseInsensitive) == 0;
    const double fillPrice = order.filledPrice > 0.0
            ? order.filledPrice
            : m_lastPrices.value(symbol, order.price);
    const int qty = order.filledQuantity;

    if (isBuy) {
        const double currentValue = pos.avgPx * pos.qty;
        pos.qty += qty;
        if (pos.qty > 0) {
            pos.avgPx = (currentValue + fillPrice * qty) / pos.qty;
        } else {
            pos.avgPx = 0.0;
        }
        m_cash -= fillPrice * qty;
    } else {
        const int remaining = pos.qty - qty;
        m_cash += fillPrice * qty;
        pos.realizedPnL += (fillPrice - pos.avgPx) * qty;
        pos.qty = remaining;
        if (pos.qty <= 0) {
            pos.avgPx = 0.0;
        }
    }

    pos.lastPrice = m_lastPrices.value(symbol, fillPrice);
    pos.unrealizedPnL = (pos.lastPrice - pos.avgPx) * pos.qty;
    m_lastPrices[symbol] = pos.lastPrice;

    if (pos.qty == 0) {
        m_positions.remove(symbol);
    } else {
        m_positions.insert(symbol, pos);
    }

    emit portfolioChanged(m_cash, totalUnrealizedPnL(), positions());
}

void PortfolioManager::updateUnrealizedFor(const QString &symbol)
{
    if (!m_positions.contains(symbol))
        return;

    Position pos = m_positions.value(symbol);
    pos.lastPrice = m_lastPrices.value(symbol, pos.avgPx);
    pos.unrealizedPnL = (pos.lastPrice - pos.avgPx) * pos.qty;
    m_positions.insert(symbol, pos);
}
