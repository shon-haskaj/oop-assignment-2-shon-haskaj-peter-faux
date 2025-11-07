#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QJsonObject>

#include "marketdataprovider.h"
#include "models/candle.h"

class StorageManager;

class ChartManager : public QObject {
    Q_OBJECT
public:
    explicit ChartManager(QObject *parent = nullptr);

    void setMarketDataProvider(MarketDataProvider *provider);
    void setStorageManager(StorageManager *storage);

    void setFeedMode(MarketDataProvider::FeedMode mode);
    MarketDataProvider::FeedMode feedMode() const { return m_mode; }

    bool startFeed(const QString &symbol);
    void stopFeed();

    QString lastSymbol() const { return m_lastSymbol; }
    double lastPrice() const { return m_lastPrice; }

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

private slots:
    void handleCandle(const Candle &c);
    void handleConnectionChange(bool connected);

private:
    void attachProvider(MarketDataProvider *provider);

    MarketDataProvider *m_provider = nullptr;
    StorageManager     *m_storage = nullptr;
    MarketDataProvider::FeedMode m_mode = MarketDataProvider::FeedMode::Synthetic;
    QString m_lastSymbol;
    double  m_lastPrice = 0.0;
};
