#include "ordermanager.h"

#include <QDateTime>

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
                                int quantity,
                                double price)
{
    Order order;
    order.id = m_nextId++;
    order.symbol = normaliseSymbol(symbol);
    order.quantity = quantity;
    order.side = side.trimmed().toUpper();
    order.type = orderTypeToString(type);
    order.price = price;
    order.status = QStringLiteral("Open");
    order.timestamp = QDateTime::currentDateTimeUtc();
    return order;
}

int OrderManager::placeOrder(OrderType type,
                             const QString &symbol,
                             const QString &side,
                             int quantity,
                             double price)
{
    if (symbol.trimmed().isEmpty() || quantity <= 0)
        return -1;

    const QString key = normaliseSymbol(symbol);
    double effectivePrice = price;
    if (type == OrderType::Market && effectivePrice <= 0.0) {
        effectivePrice = m_lastPrices.value(key, price);
    }

    Order order = createOrder(type, key, side, quantity, effectivePrice);
    if (type == OrderType::Market) {
        order.status = QStringLiteral("Filled");
        order.filledPrice = effectivePrice;
        order.filledQuantity = quantity;
    }

    m_orders.insert(order.id, order);
    emit orderPlaced(order);
    emit ordersChanged(m_orders.values());

    if (order.status == QStringLiteral("Filled")) {
        emit orderFilled(order);
    }

    return order.id;
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
