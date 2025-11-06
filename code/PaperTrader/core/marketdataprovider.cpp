#include "marketdataprovider.h"
#include <algorithm>
#include <QRandomGenerator>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>


Q_LOGGING_CATEGORY(lcMarket, "market")

MarketDataProvider::MarketDataProvider(QObject *parent)
    : QObject(parent)
{
    QObject::connect(&m_timer, &QTimer::timeout, this, [this]() {
        Candle c;
        c.symbol = "TEST";
        c.timestamp = QDateTime::currentDateTime();
        double change = (QRandomGenerator::global()->bounded(-5, 5)) / 10.0;
        c.open = m_lastPrice;
        c.close = m_lastPrice + change;
        c.high = std::max(c.open, c.close) + 0.3;
        c.low  = std::min(c.open, c.close) - 0.3;
        c.volume = QRandomGenerator::global()->bounded(50, 200);
        m_lastPrice = c.close;
        emit newCandle(c);
    });

}

void MarketDataProvider::startFeed(FeedMode mode, const QString &symbol)
{
    stopFeed();
    m_currentMode = mode;

    switch (mode) {
    case FeedMode::Synthetic:
        startSyntheticFeed();
        break;

    case FeedMode::Binance: {
        QString sym = symbol.isEmpty() ? "btcusdt" : symbol.toLower();
        startBinanceFeed(sym, "1s");
        break;
    }

    }
}

void MarketDataProvider::stopFeed()
{
    // Stop synthetic feed timer (if used)
    if (m_timer.isActive())
        m_timer.stop();

    // Stop Binance websocket (if active)
    if (m_binanceActive && m_socket.isValid()) {
        m_socket.close();
        m_binanceActive = false;
    }

    // Normalize connection state: emit once if we were connected
    if (m_connected) {
        m_connected = false;
        emit connectionStateChanged(false);
    }
}

void MarketDataProvider::startSyntheticFeed()
{
    m_timer.setInterval(1000);
    m_timer.start();
    if (!m_connected) {
        m_connected = true;
        emit connectionStateChanged(true);
    }
    qCInfo(lcMarket) << "Synthetic feed started.";
}

void MarketDataProvider::startBinanceFeed(const QString &symbol,
                                          const QString &interval)
{
    // ✅ Prevent duplicate signal connections
    QObject::disconnect(&m_socket, nullptr, this, nullptr);
    if (m_socket.isValid()) {
        m_socket.close();
    }

    QString endpoint = QStringLiteral("wss://stream.binance.com:9443/ws/%1@kline_%2")
                           .arg(symbol.toLower(), interval);

    QObject::connect(&m_socket, &QWebSocket::connected, this, [this, symbol]() {
        m_binanceActive = true;
        m_connected = true;
        emit connectionStateChanged(true);
        qCInfo(lcMarket) << "Binance connected:" << symbol;
    });

    QObject::connect(&m_socket, &QWebSocket::disconnected, this, [this]() {
        m_binanceActive = false;
        m_connected = false;
        emit connectionStateChanged(false);
        qCWarning(lcMarket) << "Binance disconnected.";
    });

    QObject::connect(&m_socket, &QWebSocket::textMessageReceived,
                     this, [this](const QString &msg) {
                         QJsonDocument doc = QJsonDocument::fromJson(msg.toUtf8());
                         if (!doc.isObject()) return;
                         QJsonObject k = doc.object().value("k").toObject();
                         if (k.isEmpty() || !k["x"].toBool()) return;  // only closed candles
                         Candle c;
                         c.symbol = doc.object().value("s").toString();
                         c.timestamp = QDateTime::fromMSecsSinceEpoch(k["t"].toVariant().toLongLong());
                         c.open  = k["o"].toString().toDouble();
                         c.high  = k["h"].toString().toDouble();
                         c.low   = k["l"].toString().toDouble();
                         c.close = k["c"].toString().toDouble();
                         c.volume = k["v"].toString().toDouble();
                         emit newCandle(c);
                     });

    qCInfo(lcMarket) << "Connecting to Binance:" << endpoint;
    m_socket.open(QUrl(endpoint));
}




