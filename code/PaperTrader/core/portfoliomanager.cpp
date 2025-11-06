#include "portfoliomanager.h"

#include <QtGlobal>
#include <cmath>
#include <algorithm>

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
    double price = order.filledPrice > 0.0
            ? order.filledPrice
            : m_lastPrices.value(symbol, order.price);
    if (price <= 0.0)
        price = order.price;
    if (price <= 0.0)
        price = m_lastPrices.value(symbol, 0.0);

    auto weightedAverage = [](double existingQty, double existingAvg, double addQty, double addPrice) {
        const double absExisting = std::abs(existingQty);
        const double total = absExisting + addQty;
        if (qFuzzyIsNull(total))
            return 0.0;
        return (existingAvg * absExisting + addPrice * addQty) / total;
    };

    double remainingQty = order.filledQuantity;

    if (isBuy) {
        while (remainingQty > 0.0) {
            if (pos.qty < 0.0) {
                const double coverQty = std::min(remainingQty, -pos.qty);
                const double realized = (pos.avgPx - price) * coverQty;
                pos.realizedPnL += realized;
                m_realizedPnL += realized;
                m_cash -= price * coverQty;
                pos.qty += coverQty;
                remainingQty -= coverQty;
                if (qFuzzyIsNull(pos.qty))
                    pos.avgPx = 0.0;
            } else {
                m_cash -= price * remainingQty;
                pos.avgPx = weightedAverage(pos.qty, pos.avgPx, remainingQty, price);
                pos.qty += remainingQty;
                remainingQty = 0.0;
            }
        }
    } else {
        while (remainingQty > 0.0) {
            if (pos.qty > 0.0) {
                const double sellQty = std::min(remainingQty, pos.qty);
                const double realized = (price - pos.avgPx) * sellQty;
                pos.realizedPnL += realized;
                m_realizedPnL += realized;
                m_cash += price * sellQty;
                pos.qty -= sellQty;
                remainingQty -= sellQty;
                if (qFuzzyIsNull(pos.qty))
                    pos.avgPx = 0.0;
            } else {
                m_cash += price * remainingQty;
                pos.avgPx = weightedAverage(pos.qty, pos.avgPx, remainingQty, price);
                pos.qty -= remainingQty;
                remainingQty = 0.0;
            }
        }
    }

    m_lastPrices[symbol] = price;
    pos.lastPrice = m_lastPrices.value(symbol, price);
    pos.unrealizedPnL = (pos.lastPrice - pos.avgPx) * pos.qty;

    if (qFuzzyIsNull(pos.qty)) {
        pos.unrealizedPnL = 0.0;
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
