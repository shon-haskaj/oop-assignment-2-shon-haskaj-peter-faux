#pragma once
#include <QObject>
#include <QTimer>
#include <QWebSocket>
#include <QLoggingCategory>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QDateTime>
#include <QUrlQuery>
#include <QProcessEnvironment>
#include "models/candle.h"

Q_DECLARE_LOGGING_CATEGORY(lcMarket)

/**
 * MarketDataProvider: provides synthetic or live market feeds.
 *
 * Supports:
 *  - Synthetic random candles
 *  - Binance live WebSocket feed
 *  - (Finnhub placeholder)
 */
class MarketDataProvider : public QObject {
    Q_OBJECT
public:
    enum class FeedMode { Synthetic, Binance, Finnhub };

    explicit MarketDataProvider(QObject *parent = nullptr);

    // Unified public feed entrypoint
    void startFeed(FeedMode mode, const QString &symbol = QString());
    void stopFeed();

    /**
     * @brief Set Finnhub API key at runtime.
     * If not set, the provider will also check FINNHUB_API_KEY from the environment.
     * @param token Finnhub REST API token.
     */
    void setFinnhubApiKey(const QString& token);

signals:
    void newCandle(const Candle &c);
    void connectionStateChanged(bool connected);
    /**
     * @brief Emitted when a provider-level error occurs (network/auth/rate-limit).
     */
    void errorOccurred(const QString& message);

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

    FeedMode m_currentMode = FeedMode::Synthetic;

    // ===== Finnhub state =====
    QNetworkAccessManager m_http;
    QTimer m_poll;
    QString m_finnhubApiKey;
    QString m_finnhubSymbol;
    QString m_finnhubDisplaySymbol;
    QString m_finnhubResolution;   // "1","5","15","30","60","D","W","M"
    qint64  m_lastEpoch = 0;       // last fully processed candle end time (UTC seconds)
    int     m_resSec    = 60;      // resolved resolution in seconds (e.g., "1" -> 60)
    int     m_pollMs    = 5000;    // current poll interval; will backoff on 429 up to 60000
    bool    m_connected = false;

    // ===== Finnhub helpers =====
    void startFinnhubFeed(const QString& symbol);
    void stopFinnhubFeed();
    void pollFinnhub();  // timer slot
    void requestFinnhubCandles(qint64 from, qint64 to);
    void handleFinnhubReply(QNetworkReply* reply);
    QString mapSymbolToFinnhub(const QString& symbol) const;
    void chooseDefaultResolution();     // sets m_finnhubResolution + m_resSec
    QString finnhubCandlePath() const;  // "crypto/candle" or "stock/candle"
    void setConnected(bool on);
    void applyBackoff(int newMs);
};
