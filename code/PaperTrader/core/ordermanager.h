#pragma once
#include <QObject>
#include <QMap>
#include <QList>
#include <QHash>
#include "models/order.h"

class OrderManager : public QObject {
    Q_OBJECT
public:
    enum class OrderType { Market, Limit };

    explicit OrderManager(QObject *parent = nullptr);

    Order createOrder(OrderType type,
                      const QString &symbol,
                      const QString &side,
                      int quantity,
                      double price = 0.0);

    int placeOrder(OrderType type,
                   const QString &symbol,
                   const QString &side,
                   int quantity,
                   double price = 0.0);

    bool cancelOrder(int orderId);

    QList<Order> orders() const { return m_orders.values(); }

    void setLastPrice(const QString &symbol, double price);

signals:
    void ordersChanged(const QList<Order> &orders);
    void orderPlaced(const Order &order);
    void orderCancelled(const Order &order);
    void orderFilled(const Order &order);

private:
    QString orderTypeToString(OrderType type) const;
    QString normaliseSymbol(const QString &symbol) const;

    int m_nextId = 1;
    QMap<int, Order> m_orders;
    QHash<QString, double> m_lastPrices;
};
