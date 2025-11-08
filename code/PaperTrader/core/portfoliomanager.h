#pragma once
#include <QObject>
#include <QMap>
#include <QList>
#include "models/position.h"
#include "models/order.h"
#include "models/candle.h"
#include "models/portfoliosnapshot.h"
#include "models/quote.h"

class PortfolioManager : public QObject {
    Q_OBJECT
public:
    explicit PortfolioManager(QObject *parent = nullptr);

    struct OrderValidationResult {
        bool accepted = false;
        bool partial = false;
        double acceptedQuantity = 0.0;
        double effectivePrice = 0.0;
        double fee = 0.0;
        QString errorCode;
    };

    double cash() const { return m_cash; }
    QList<Position> positions() const { return m_positions.values(); }
    double totalUnrealizedPnL() const;
    double realizedPnL() const { return m_realizedPnL; }
    PortfolioSnapshot snapshot() const;

    OrderValidationResult validateOrder(bool isMarket,
                                        const QString &symbol,
                                        const QString &side,
                                        double quantity,
                                        double price) const;
    double estimateFee(double price, double quantity) const;

public slots:
    void onCandle(const Candle &c);
    void applyFill(const Order &order);
    void onOrdersUpdated(const QList<Order> &orders);
    void updateFromQuote(const Quote &quote);

signals:
    void portfolioChanged(const PortfolioSnapshot &snapshot, const QList<Position> &positions);

private:
    void updateUnrealizedFor(const QString &symbol);
    double marginForPosition(const Position &position) const;
    double marginForOrder(const Order &order) const;
    double marginForOrder(const QString &symbol,
                          const QString &side,
                          double quantity,
                          double price) const;
    double openingQuantityForOrder(const QString &symbol,
                                   const QString &side,
                                   double quantity) const;
    double availableFundsInternal() const;
    void recomputeOrderMargin();
    void recordOrUpdatePosition(const QString &symbol, const Position &position);
    void emitSnapshot();

    double m_cash = 100000.0;
    QMap<QString, Position> m_positions;
    QMap<QString, double> m_lastPrices;
    QList<Order> m_openOrders;
    double m_realizedPnL = 0.0;
    double m_orderMargin = 0.0;
    double m_shortMarginRate = 0.5;
    double m_feeRate = 0.0004;
};
