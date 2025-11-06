#pragma once
#include <QObject>
#include <QMap>
#include <QList>
#include "models/position.h"
#include "models/order.h"
#include "models/candle.h"

class PortfolioManager : public QObject {
    Q_OBJECT
public:
    explicit PortfolioManager(QObject *parent = nullptr);

    double cash() const { return m_cash; }
    QList<Position> positions() const { return m_positions.values(); }
    double totalUnrealizedPnL() const;

public slots:
    void onCandle(const Candle &c);
    void applyFill(const Order &order);

signals:
    void portfolioChanged(double cash, double unrealizedPnL, const QList<Position> &positions);

private:
    void updateUnrealizedFor(const QString &symbol);

    double m_cash = 100000.0;
    QMap<QString, Position> m_positions;
    QMap<QString, double> m_lastPrices;
};
