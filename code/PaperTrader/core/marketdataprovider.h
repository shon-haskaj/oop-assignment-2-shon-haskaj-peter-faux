#pragma once
#include <QObject>
#include <QTimer>
#include <QWebSocket>
#include <QLoggingCategory>
#include "models/candle.h"

Q_DECLARE_LOGGING_CATEGORY(lcMarket)

/**
 * MarketDataProvider: provides synthetic or live market feeds.
 *
 * Supports:
 *  - Synthetic random candles
 *  - Binance live WebSocket feed
 */
class MarketDataProvider : public QObject {
    Q_OBJECT
public:
    enum class FeedMode { Synthetic, Binance };

    explicit MarketDataProvider(QObject *parent = nullptr);

    // Unified public feed entrypoint
    void startFeed(FeedMode mode, const QString &symbol = QString());
    void stopFeed();

signals:
    void newCandle(const Candle &c);
    void connectionStateChanged(bool connected);

private:
    // ---- Synthetic feed ----
    void startSyntheticFeed();
    QTimer m_timer;
    double m_lastPrice = 20000.0;

    // ---- Binance feed ----
    void startBinanceFeed(const QString &symbol = "btcusdt",
                          const QString &interval = "1s");
    QWebSocket m_socket;
    bool m_binanceActive = false;
    bool m_connected = false;

    FeedMode m_currentMode = FeedMode::Synthetic;
};
