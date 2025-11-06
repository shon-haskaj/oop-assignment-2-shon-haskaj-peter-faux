#include "portfoliomanager.h"

#include <QtGlobal>
#include <cmath>

namespace {
constexpr double kMaintenanceMarginRate = 0.5; // Simplified margin assumption
}

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

PortfolioSnapshot PortfolioManager::snapshot() const
{
    PortfolioSnapshot snap;
    snap.accountBalance = m_cash;
    snap.realizedPnL = m_realizedPnL;
    snap.unrealizedPnL = totalUnrealizedPnL();

    double marketValue = 0.0;
    double margin = 0.0;
    for (const Position &pos : m_positions) {
        marketValue += pos.qty * pos.lastPrice;
        margin += marginForPosition(pos);
    }

    double orderMargin = 0.0;
    for (const Order &order : m_openOrders) {
        orderMargin += marginForOrder(order);
    }

    // Equity, margins, and liquidity mirror a simplified retail margin account.
    snap.equity = m_cash + marketValue;
    snap.accountMargin = margin;
    snap.orderMargin = orderMargin;
    snap.availableFunds = snap.equity - snap.accountMargin - snap.orderMargin;
    return snap;
}

void PortfolioManager::onCandle(const Candle &c)
{
    const QString symbol = c.symbol.toUpper();
    m_lastPrices[symbol] = c.close;
    updateUnrealizedFor(symbol);
    emitSnapshot();
}

void PortfolioManager::applyFill(const Order &order)
{
    if (order.filledQuantity <= 0.0)
        return;

    const QString symbol = order.symbol.toUpper();
    Position pos = m_positions.value(symbol);
    pos.symbol = symbol;

    const bool isBuy = order.side.compare("BUY", Qt::CaseInsensitive) == 0;
    const double referencePrice = order.filledPrice > 0.0
            ? order.filledPrice
            : m_lastPrices.value(symbol, order.price);
    const double qty = order.filledQuantity;

    if (isBuy) {
        const double currentValue = pos.avgPx * pos.qty;
        pos.qty += qty;
        if (!qFuzzyIsNull(pos.qty)) {
            pos.avgPx = (currentValue + referencePrice * qty) / pos.qty;
        } else {
            pos.avgPx = 0.0;
        }
        m_cash -= referencePrice * qty;
    } else {
        m_cash += referencePrice * qty;
        const double realizedDelta = (referencePrice - pos.avgPx) * qty;
        pos.realizedPnL += realizedDelta;
        // Track realized P&L at the portfolio level even when positions are closed.
        m_realizedPnL += realizedDelta;
        pos.qty -= qty;
        if (qFuzzyIsNull(pos.qty)) {
            pos.avgPx = 0.0;
        }
    }

    pos.lastPrice = m_lastPrices.value(symbol, referencePrice);
    pos.unrealizedPnL = (pos.lastPrice - pos.avgPx) * pos.qty;
    m_lastPrices[symbol] = pos.lastPrice;

    if (qFuzzyIsNull(pos.qty)) {
        // Preserve realized P&L in the manager before removing the empty slot
        m_positions.remove(symbol);
    } else {
        m_positions.insert(symbol, pos);
    }

    emitSnapshot();
}

void PortfolioManager::onOrdersUpdated(const QList<Order> &orders)
{
    m_openOrders.clear();
    for (const Order &order : orders) {
        if (order.status.compare(QStringLiteral("Open"), Qt::CaseInsensitive) == 0) {
            m_openOrders.append(order);
        }
    }
    // Pending orders reserve margin so that available funds reflect true buying power.
    emitSnapshot();
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

double PortfolioManager::marginForPosition(const Position &position) const
{
    const double notional = std::abs(position.qty * position.lastPrice);
    return notional * kMaintenanceMarginRate;
}

double PortfolioManager::marginForOrder(const Order &order) const
{
    const QString symbol = order.symbol.toUpper();
    const double price = order.price > 0.0
            ? order.price
            : m_lastPrices.value(symbol, 0.0);
    const double notional = std::abs(order.quantity * price);
    return notional * kMaintenanceMarginRate;
}

void PortfolioManager::emitSnapshot()
{
    emit portfolioChanged(snapshot(), positions());
}
