#include "ordermanager.h"

#include <QDateTime>
#include <algorithm>

#include "portfoliomanager.h"

OrderManager::OrderManager(QObject *parent)
    : QObject(parent) {}

QString OrderManager::orderTypeToString(OrderType type) const
{
    switch (type) {
    case OrderType::Market: return QStringLiteral("Market");
    case OrderType::Limit:  return QStringLiteral("Limit");
    }
    return QStringLiteral("Unknown");
}

QString OrderManager::normaliseSymbol(const QString &symbol) const
{
    return symbol.trimmed().toUpper();
}

Order OrderManager::createOrder(OrderType type,
                                const QString &symbol,
                                const QString &side,
                                double quantity,
                                double price)
{
    Order order;
    order.id = m_nextId++;
    order.symbol = normaliseSymbol(symbol);
    order.quantity = quantity;
    order.requestedQuantity = quantity;
    order.side = side.trimmed().toUpper();
    order.type = orderTypeToString(type);
    order.price = price;
    order.status = QStringLiteral("Open");
    order.timestamp = QDateTime::currentDateTimeUtc();
    return order;
}

OrderManager::OrderPlacementResult OrderManager::placeOrder(OrderType type,
                                                            const QString &symbol,
                                                            const QString &side,
                                                            double quantity,
                                                            double price)
{
    OrderPlacementResult result;
    result.rejectedQuantity = quantity;

    const QString key = normaliseSymbol(symbol);
    if (key.isEmpty()) {
        result.errorCode = QStringLiteral("ERR_INVALID_SYMBOL");
        emit orderRejected(symbol, result.errorCode, quantity);
        return result;
    }

    if (quantity <= 0.0) {
        result.errorCode = QStringLiteral("ERR_INVALID_QTY");
        emit orderRejected(key, result.errorCode, quantity);
        return result;
    }

    const bool isMarket = type == OrderType::Market;
    PortfolioManager::OrderValidationResult validation;

    if (m_portfolio) {
        validation = m_portfolio->validateOrder(isMarket, key, side, quantity, price);
        if (!validation.accepted) {
            result.errorCode = validation.errorCode;
            emit orderRejected(key, validation.errorCode, quantity);
            return result;
        }
    } else {
        validation.accepted = true;
        validation.acceptedQuantity = quantity;
        validation.effectivePrice = (isMarket && price <= 0.0)
                ? m_lastPrices.value(key, price)
                : price;
    }

    double effectivePrice = validation.effectivePrice;
    if (isMarket && effectivePrice <= 0.0) {
        effectivePrice = m_lastPrices.value(key, price);
    }
    if (!isMarket && effectivePrice <= 0.0)
        effectivePrice = price;

    Order order = createOrder(type, key, side, quantity, effectivePrice);
    order.quantity = validation.acceptedQuantity;
    order.errorCode = validation.errorCode;

    if (isMarket) {
        order.status = validation.partial
                ? QStringLiteral("PartiallyFilled")
                : QStringLiteral("Filled");
        order.filledPrice = effectivePrice;
        order.filledQuantity = validation.acceptedQuantity;
        order.fee = validation.fee;
    }

    m_orders.insert(order.id, order);
    emit orderPlaced(order);
    emit ordersChanged(m_orders.values());

    result.accepted = true;
    result.partial = validation.partial;
    result.order = order;
    result.errorCode = validation.errorCode;
    result.rejectedQuantity = std::max(0.0, quantity - validation.acceptedQuantity);

    if (order.status.compare(QStringLiteral("Filled"), Qt::CaseInsensitive) == 0
            || order.status.compare(QStringLiteral("PartiallyFilled"), Qt::CaseInsensitive) == 0) {
        emit orderFilled(order);
    }

    if (validation.partial && !validation.errorCode.isEmpty()) {
        emit orderRejected(key, validation.errorCode, result.rejectedQuantity);
    }

    return result;
}

bool OrderManager::cancelOrder(int orderId)
{
    if (!m_orders.contains(orderId))
        return false;

    Order order = m_orders.value(orderId);
    if (order.status == QStringLiteral("Filled"))
        return false;

    order.status = QStringLiteral("Cancelled");
    m_orders.insert(orderId, order);
    emit orderCancelled(order);
    emit ordersChanged(m_orders.values());
    return true;
}

void OrderManager::setLastPrice(const QString &symbol, double price)
{
    m_lastPrices.insert(normaliseSymbol(symbol), price);
}

void OrderManager::setPortfolioManager(PortfolioManager *manager)
{
    m_portfolio = manager;
}
