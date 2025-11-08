#pragma once
#include <QObject>
#include <QMap>
#include "models/candle.h"
#include "models/order.h"

class OrderManager;
class PortfolioManager;

class ExecutionSimulator : public QObject {
    Q_OBJECT
public:
    explicit ExecutionSimulator(QObject *parent = nullptr);

    void setOrderManager(OrderManager *manager);
    void setPortfolioManager(PortfolioManager *manager);

public slots:
    void onCandle(const Candle &candle);
    void onOrdersChanged(const QList<Order> &orders);

private:
    void tryFill(const Candle &candle);
    bool shouldFill(const Order &order, const Candle &candle, double &fillPrice) const;

    OrderManager *m_orderManager = nullptr;
    PortfolioManager *m_portfolioManager = nullptr;
    QMap<int, Order> m_openLimitOrders;
};
