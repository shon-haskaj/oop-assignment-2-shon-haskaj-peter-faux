#pragma once
#include <QObject>
#include <QStringList>
#include <QString>
#include <QJsonObject>
#include "core/chartmanager.h"
#include "core/models/candle.h"

class ChartController : public QObject {
    Q_OBJECT
public:
    explicit ChartController(ChartManager *chartManager, QObject *parent = nullptr);

    void setFeedMode(MarketDataProvider::FeedMode mode);
    MarketDataProvider::FeedMode feedMode() const
    {
        return m_chartManager ? m_chartManager->feedMode() : m_mode;
    }

    bool startFeed(const QString &symbol);
    void stopFeed();

    double lastPrice() const;
    Quote lastQuote() const;
    QString lastSymbol() const;

    QStringList loadWatchlist() const;
    void saveWatchlist(const QStringList &symbols) const;
    QJsonObject loadSettings() const;
    void saveSettings(const QJsonObject &settings) const;

signals:
    void candleReceived(const Candle &c);
    void connectionStateChanged(bool connected);
    void feedStarted(const QString &symbol, MarketDataProvider::FeedMode mode);
    void feedStopped();
    void lastPriceChanged(const QString &symbol, double price);
    void quoteUpdated(const Quote &quote);

private:
    ChartManager *m_chartManager = nullptr;
    MarketDataProvider::FeedMode m_mode = MarketDataProvider::FeedMode::Synthetic;
};
