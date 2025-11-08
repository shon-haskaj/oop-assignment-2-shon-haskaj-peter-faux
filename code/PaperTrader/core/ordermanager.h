#pragma once
#include <QObject>
#include <QMap>
#include <QList>
#include <QHash>
#include "models/order.h"

class PortfolioManager;

class OrderManager : public QObject {
    Q_OBJECT
public:
    enum class OrderType { Market, Limit };

    struct OrderPlacementResult {
        bool accepted = false;
        bool partial = false;
        Order order;
        QString errorCode;
        double rejectedQuantity = 0.0;
    };

    explicit OrderManager(QObject *parent = nullptr);

    Order createOrder(OrderType type,
                      const QString &symbol,
                      const QString &side,
                      double quantity,
                      double price = 0.0);

    OrderPlacementResult placeOrder(OrderType type,
                                    const QString &symbol,
                                    const QString &side,
                                    double quantity,
                                    double price = 0.0);

    bool cancelOrder(int orderId);
    void applyFill(int orderId, double price, double quantity, double fee = 0.0);

    QList<Order> orders() const { return m_orders.values(); }

    void setLastPrice(const QString &symbol, double price);
    void setPortfolioManager(PortfolioManager *manager);

signals:
    void ordersChanged(const QList<Order> &orders);
    void orderPlaced(const Order &order);
    void orderCancelled(const Order &order);
    void orderFilled(const Order &order);
    void orderRejected(const QString &symbol, const QString &errorCode, double rejectedQuantity);

private:
    QString orderTypeToString(OrderType type) const;
    QString normaliseSymbol(const QString &symbol) const;

    int m_nextId = 1;
    QMap<int, Order> m_orders;
    QHash<QString, double> m_lastPrices;
    PortfolioManager *m_portfolio = nullptr;
};
