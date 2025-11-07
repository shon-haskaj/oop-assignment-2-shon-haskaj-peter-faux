#pragma once
#include <QObject>
#include <QList>
#include <QString>
#include "core/ordermanager.h"
#include "core/portfoliomanager.h"
#include "core/models/order.h"
#include "core/models/position.h"
#include "core/models/portfoliosnapshot.h"

class PaperTraderApp;

class TradingController : public QObject {
    Q_OBJECT
public:
    explicit TradingController(PaperTraderApp *app, QObject *parent = nullptr);

    QList<Order> orders() const;
    PortfolioSnapshot snapshot() const;
    QList<Position> positions() const;

    OrderManager::OrderPlacementResult placeOrder(OrderManager::OrderType type,
                                                  const QString &symbol,
                                                  const QString &side,
                                                  double quantity,
                                                  double price);
    bool cancelOrder(int orderId);

public slots:
    void onLastPriceChanged(const QString &symbol, double price);

signals:
    void ordersChanged(const QList<Order> &orders);
    void orderRejected(const QString &symbol, const QString &errorCode, double rejectedQuantity);
    void portfolioChanged(const PortfolioSnapshot &snapshot, const QList<Position> &positions);

private:
    void bindOrderManager(OrderManager *manager);
    void bindPortfolioManager(PortfolioManager *manager);

    PaperTraderApp *m_app = nullptr;
    OrderManager *m_orderManager = nullptr;
    PortfolioManager *m_portfolioManager = nullptr;
};
