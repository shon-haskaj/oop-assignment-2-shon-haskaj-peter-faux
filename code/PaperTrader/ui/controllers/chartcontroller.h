#pragma once
#include <QObject>
#include <QStringList>
#include <QString>
#include <QJsonObject>
#include "core/papertraderapp.h"
#include "core/models/candle.h"

class StorageManager;

class ChartController : public QObject {
    Q_OBJECT
public:
    explicit ChartController(PaperTraderApp *app, QObject *parent = nullptr);

    void setFeedMode(MarketDataProvider::FeedMode mode);
    MarketDataProvider::FeedMode feedMode() const { return m_mode; }

    bool startFeed(const QString &symbol);
    void stopFeed();

    double lastPrice() const { return m_lastPrice; }
    QString lastSymbol() const { return m_lastSymbol; }

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

    PaperTraderApp *m_app = nullptr;
    MarketDataProvider *m_provider = nullptr;
    StorageManager *m_storage = nullptr;
    MarketDataProvider::FeedMode m_mode = MarketDataProvider::FeedMode::Synthetic;
    QString m_lastSymbol;
    double m_lastPrice = 0.0;
};
