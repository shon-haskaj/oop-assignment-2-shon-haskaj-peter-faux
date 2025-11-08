#include "executionsimulator.h"

#include <QLoggingCategory>
#include <algorithm>

#include "ordermanager.h"
#include "portfoliomanager.h"

Q_LOGGING_CATEGORY(lcExecSim, "execsim")

ExecutionSimulator::ExecutionSimulator(QObject *parent)
    : QObject(parent)
{
}

void ExecutionSimulator::setOrderManager(OrderManager *manager)
{
    if (m_orderManager == manager)
        return;

    if (m_orderManager) {
        disconnect(m_orderManager, nullptr, this, nullptr);
    }

    m_orderManager = manager;
    if (m_orderManager) {
        connect(m_orderManager, &OrderManager::ordersChanged,
                this, &ExecutionSimulator::onOrdersChanged);
        onOrdersChanged(m_orderManager->orders());
    } else {
        m_openLimitOrders.clear();
    }
}

void ExecutionSimulator::setPortfolioManager(PortfolioManager *manager)
{
    m_portfolioManager = manager;
}

void ExecutionSimulator::onCandle(const Candle &candle)
{
    if (candle.symbol.isEmpty() || m_orderManager == nullptr)
        return;

    tryFill(candle);
}

void ExecutionSimulator::onOrdersChanged(const QList<Order> &orders)
{
    m_openLimitOrders.clear();
    for (const Order &order : orders) {
        const bool isLimit = order.type.compare(QStringLiteral("Limit"), Qt::CaseInsensitive) == 0;
        const bool hasQty = order.quantity > 0.0;
        const bool isActive = order.status.compare(QStringLiteral("Cancelled"), Qt::CaseInsensitive) != 0
                && order.status.compare(QStringLiteral("Filled"), Qt::CaseInsensitive) != 0;
        if (isLimit && hasQty && isActive) {
            m_openLimitOrders.insert(order.id, order);
        }
    }
}

void ExecutionSimulator::tryFill(const Candle &candle)
{
    if (m_openLimitOrders.isEmpty())
        return;

    const QString symbol = candle.symbol.toUpper();
    const auto orders = m_openLimitOrders;
    for (const Order &order : orders) {
        if (order.symbol.compare(symbol, Qt::CaseInsensitive) != 0)
            continue;

        double fillPrice = 0.0;
        if (!shouldFill(order, candle, fillPrice))
            continue;

        const double fillQty = order.quantity;
        double fee = 0.0;
        if (m_portfolioManager) {
            fee = m_portfolioManager->estimateFee(fillPrice, fillQty);
        }

        m_orderManager->applyFill(order.id, fillPrice, fillQty, fee);
    }
}

bool ExecutionSimulator::shouldFill(const Order &order, const Candle &candle, double &fillPrice) const
{
    const bool isBuy = order.side.compare(QStringLiteral("BUY"), Qt::CaseInsensitive) == 0;
    const bool isSell = order.side.compare(QStringLiteral("SELL"), Qt::CaseInsensitive) == 0;
    if (!isBuy && !isSell)
        return false;

    const double limitPrice = order.price;
    if (limitPrice <= 0.0)
        return false;

    const double high = candle.high > 0.0 ? candle.high : candle.close;
    const double low = candle.low > 0.0 ? candle.low : candle.close;

    if (isBuy) {
        if (low <= limitPrice) {
            fillPrice = std::min(limitPrice, candle.close > 0.0 ? candle.close : limitPrice);
            return true;
        }
        return false;
    }

    if (isSell) {
        if (high >= limitPrice) {
            fillPrice = std::max(limitPrice, candle.close > 0.0 ? candle.close : limitPrice);
            return true;
        }
        return false;
    }

    return false;
}
