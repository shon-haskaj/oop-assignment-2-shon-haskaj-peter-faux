#pragma once
#include <QObject>
#include <QMap>
#include <QList>
#include "models/position.h"
#include "models/order.h"
#include "models/candle.h"
#include "models/portfoliosnapshot.h"

class PortfolioManager : public QObject {
    Q_OBJECT
public:
    explicit PortfolioManager(QObject *parent = nullptr);

    double cash() const { return m_cash; }
    QList<Position> positions() const { return m_positions.values(); }
    double totalUnrealizedPnL() const;
    double realizedPnL() const { return m_realizedPnL; }
    PortfolioSnapshot snapshot() const;

public slots:
    void onCandle(const Candle &c);
    void applyFill(const Order &order);
    void onOrdersUpdated(const QList<Order> &orders);

signals:
    void portfolioChanged(const PortfolioSnapshot &snapshot, const QList<Position> &positions);

private:
    void updateUnrealizedFor(const QString &symbol);
    double marginForPosition(const Position &position) const;
    double marginForOrder(const Order &order) const;
    void emitSnapshot();

    double m_cash = 100000.0;
    QMap<QString, Position> m_positions;
    QMap<QString, double> m_lastPrices;
    QList<Order> m_openOrders;
    double m_realizedPnL = 0.0;
};
