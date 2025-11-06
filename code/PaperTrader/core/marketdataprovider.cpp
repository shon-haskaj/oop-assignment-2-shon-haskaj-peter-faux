#include "marketdataprovider.h"
#include <QRandomGenerator>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QtGlobal>
#include <QSslSocket>
#include <QSslError>


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

// Surface provider-level errors in logs (optional but helpful)
    connect(this, &MarketDataProvider::errorOccurred, this, [](const QString& msg){
        qCWarning(lcMarket) << "MarketDataProvider error:" << msg;
    });

    // Log SSL errors so HTTPS problems are obvious
    connect(&m_http, &QNetworkAccessManager::sslErrors,
            this, [](QNetworkReply* reply, const QList<QSslError>& errors){
                QStringList msgs; for (const auto& e : errors) msgs << e.errorString();
                qCWarning(lcMarket) << "SSL errors:" << msgs.join("; ");
                // TEMP for debugging: ignore to see server response; remove if you prefer strict TLS
                if (reply) reply->ignoreSslErrors();
    });


    // Finnhub poll timer setup
    m_poll.setSingleShot(false);
    connect(&m_poll, &QTimer::timeout, this, &MarketDataProvider::pollFinnhub);
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

    case FeedMode::Finnhub:
        startFinnhubFeed(symbol);
        break;
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

    // Stop Finnhub polling timer (if active)
    if (m_poll.isActive())
        m_poll.stop();

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
        emit connectionStateChanged(true);
        qCInfo(lcMarket) << "Binance connected:" << symbol;
    });

    QObject::connect(&m_socket, &QWebSocket::disconnected, this, [this]() {
        m_binanceActive = false;
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

void MarketDataProvider::setFinnhubApiKey(const QString& token)
{
    m_finnhubApiKey = token.trimmed();
}

void MarketDataProvider::startFinnhubFeed(const QString& symbol)
{

    if (!QSslSocket::supportsSsl()) {
        qCWarning(lcMarket) << "OpenSSL not available. Copy libssl-3-x64.dll and libcrypto-3-x64.dll next to your EXE.";
    }
    #ifdef FINNHUB_KEY
        if (m_finnhubApiKey.isEmpty())
            m_finnhubApiKey = QString::fromLatin1(FINNHUB_KEY).trimmed();
    #endif

    if (!m_finnhubApiKey.isEmpty()) {
        const auto masked = (m_finnhubApiKey.size() > 6)
        ? (m_finnhubApiKey.left(3) + "****" + m_finnhubApiKey.right(3))
        : QStringLiteral("***");
        qCInfo(lcMarket) << "Finnhub API key set (masked):" << masked;
    } else {
        qCWarning(lcMarket) << "Finnhub: no API key set";
    }

    // Resolve API key (runtime setter takes precedence; fallback to env var)
    if (m_finnhubApiKey.isEmpty()) {
        const auto env = QProcessEnvironment::systemEnvironment();
        m_finnhubApiKey = env.value(QStringLiteral("FINNHUB_API_KEY")).trimmed();
    }

    if (m_finnhubApiKey.isEmpty()) {
        emit errorOccurred(QStringLiteral("Finnhub: API key missing. Set FINNHUB_API_KEY or call setFinnhubApiKey()."));
        setConnected(false);
        return;
    }

    // Normalize / map symbol
    const QString raw = symbol.trimmed();
    m_finnhubSymbol = mapSymbolToFinnhub(raw);

    // Display symbol should match what the UI expects (e.g., no exchange prefix for crypto)
    m_finnhubDisplaySymbol = raw;
    if (m_finnhubDisplaySymbol.contains(':')) {
        // If user typed BINANCE:BTCUSDT, strip prefix for display to match Binance’s style in your chart
        m_finnhubDisplaySymbol = m_finnhubDisplaySymbol.mid(m_finnhubDisplaySymbol.indexOf(':') + 1);
    }
    // Normalize like Binance path: uppercase without prefix
    m_finnhubDisplaySymbol = m_finnhubDisplaySymbol.toUpper();


    // Choose default resolution (maps to m_finnhubResolution + m_resSec)
    chooseDefaultResolution();

    // Bootstrap window: last hour for intraday; 1 trading day for daily
    const qint64 nowSec = QDateTime::currentDateTimeUtc().toSecsSinceEpoch();
    const bool isDaily = (m_finnhubResolution == QLatin1String("D") ||
                          m_finnhubResolution == QLatin1String("W") ||
                          m_finnhubResolution == QLatin1String("M"));

    const qint64 bootstrapWindow = isDaily ? (24 * 3600) : (60 * 60);
    m_lastEpoch = nowSec - bootstrapWindow;

    // Initial fetch
    requestFinnhubCandles(m_lastEpoch, nowSec);

    // Start polling
    m_pollMs = 5000;
    m_poll.start(m_pollMs);

    qCInfo(lcMarket) << "Finnhub start:"
                     << "requestSymbol=" << m_finnhubSymbol
                     << "displaySymbol=" << m_finnhubDisplaySymbol
                     << "resolution=" << m_finnhubResolution;
}

void MarketDataProvider::stopFinnhubFeed()
{
    if (m_poll.isActive())
        m_poll.stop();
    setConnected(false);
    // Keep symbol/resolution; harmless if user resumes quickly
}

void MarketDataProvider::pollFinnhub()
{
    const qint64 nowSec = QDateTime::currentDateTimeUtc().toSecsSinceEpoch();
    const qint64 from   = m_lastEpoch + m_resSec;
    const qint64 to     = nowSec;

    if (to <= from) {
        return; // nothing new yet
    }
    requestFinnhubCandles(from, to);
}

void MarketDataProvider::requestFinnhubCandles(qint64 from, qint64 to)
{
    if (m_finnhubApiKey.isEmpty() || m_finnhubSymbol.isEmpty() || m_finnhubResolution.isEmpty()) {
        emit errorOccurred(QStringLiteral("Finnhub: request aborted due to missing state."));
        return;
    }

    const QString path = QStringLiteral("https://finnhub.io/api/v1/%1").arg(finnhubCandlePath());
    QUrl url(path);
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("symbol"),     m_finnhubSymbol);
    q.addQueryItem(QStringLiteral("resolution"), m_finnhubResolution);
    q.addQueryItem(QStringLiteral("from"),       QString::number(from));
    q.addQueryItem(QStringLiteral("to"),         QString::number(to));
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::KnownHeaders::ContentTypeHeader, QStringLiteral("application/json"));
    req.setRawHeader("Accept", "application/json");
    // Use header auth to avoid logging token in URL
    req.setRawHeader("X-Finnhub-Token", m_finnhubApiKey.toUtf8());

    qCInfo(lcMarket) << "Finnhub request"
                     << req.url().toString()
                     << "resolution=" << m_finnhubResolution
                     << "from=" << from << "to=" << to;

    QNetworkReply* reply = m_http.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]{
        handleFinnhubReply(reply);
    });
}


void MarketDataProvider::handleFinnhubReply(QNetworkReply* reply)
{
    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QNetworkReply::NetworkError netErr = reply->error();

    auto cleanup = qScopeGuard([&](){
        reply->deleteLater();
    });

    // Treat non-2xx HTTP as errors even if Qt returns NoError
    if (httpStatus < 200 || httpStatus >= 300) {
        const QByteArray body = reply->readAll();
        if (httpStatus == 429) {
            // Rate limit: backoff (max 60s)
            const int newMs = qMin(m_pollMs * 2, 60000);
            applyBackoff(newMs);
            qCWarning(lcMarket) << "Finnhub HTTP 429. Backoff to (ms):" << m_pollMs;
            emit errorOccurred(QStringLiteral("Finnhub: rate-limited (429). Backing off to %1 ms.").arg(m_pollMs));
        } else {
            qCWarning(lcMarket) << "Finnhub HTTP error" << httpStatus << "body:" << body;
            emit errorOccurred(QStringLiteral("Finnhub HTTP %1. See logs for body.").arg(httpStatus));
        }
        return;
    }

    // Network-level error
    if (netErr != QNetworkReply::NoError) {
        const QByteArray errBody = reply->readAll();
        qCWarning(lcMarket) << "Finnhub network error" << httpStatus << reply->errorString() << errBody;
        emit errorOccurred(QStringLiteral("Finnhub network error (%1): %2")
                               .arg(httpStatus)
                               .arg(reply->errorString()));
        return;
    }

    qCInfo(lcMarket) << "Finnhub reply status:" << httpStatus;

    const QByteArray body = reply->readAll();
    const QJsonDocument doc = QJsonDocument::fromJson(body);
    if (!doc.isObject()) {
        emit errorOccurred(QStringLiteral("Finnhub: invalid JSON payload."));
        return;
    }

    const QJsonObject obj = doc.object();

    // --- Branch on Finnhub status field ("s") ---
    const QString status = obj.value(QStringLiteral("s")).toString();
    const int pointCount = obj.value(QStringLiteral("t")).toArray().size();
    qCInfo(lcMarket) << "Finnhub payload status:" << status << "points:" << pointCount;

    if (status == QLatin1String("ok")) {
        // proceed

    } else if (status == QLatin1String("no_data")) {
        qCInfo(lcMarket) << "Finnhub: no_data in window. Waiting for next poll...";
        return;

    } else {
        const QString errMsg =
            obj.value(QStringLiteral("error")).toString().trimmed().isEmpty()
                ? obj.value(QStringLiteral("errmsg")).toString().trimmed()
                : obj.value(QStringLiteral("error")).toString().trimmed();

        qCWarning(lcMarket) << "Finnhub: unexpected status:" << status
                            << (errMsg.isEmpty() ? QString() : QStringLiteral(" error: %1").arg(errMsg));

        emit errorOccurred(errMsg.isEmpty()
                               ? QStringLiteral("Finnhub: API returned status '%1'.").arg(status)
                               : QStringLiteral("Finnhub error: %1").arg(errMsg));
        return;
    }

    // Parse arrays
    const QJsonArray t = obj.value(QStringLiteral("t")).toArray();
    const QJsonArray o = obj.value(QStringLiteral("o")).toArray();
    const QJsonArray h = obj.value(QStringLiteral("h")).toArray();
    const QJsonArray l = obj.value(QStringLiteral("l")).toArray();
    const QJsonArray c = obj.value(QStringLiteral("c")).toArray();
    const QJsonArray v = obj.value(QStringLiteral("v")).toArray();

    const int n = t.size();
    if (!(o.size()==n && h.size()==n && l.size()==n && c.size()==n && v.size()==n)) {
        emit errorOccurred(QStringLiteral("Finnhub: mismatched OHLCV array lengths."));
        return;
    }

    // Successful response -> if we had backed off, reset to default
    if (m_pollMs != 5000) {
        m_pollMs = 5000;
        if (m_poll.isActive()) m_poll.setInterval(m_pollMs);
    }
    if (!m_connected) setConnected(true);

    // Emit candles in ascending time
    for (int i = 0; i < n; ++i) {
        const qint64 tsSec = static_cast<qint64>(t.at(i).toDouble());
        const double oV = o.at(i).toDouble();
        const double hV = h.at(i).toDouble();
        const double lV = l.at(i).toDouble();
        const double cV = c.at(i).toDouble();
        const double vV = v.at(i).toDouble();

        Candle candle;
        candle.symbol    = m_finnhubDisplaySymbol;
        candle.timestamp = QDateTime::fromMSecsSinceEpoch(tsSec * 1000, Qt::UTC);
        candle.open      = oV;
        candle.high      = hV;
        candle.low       = lV;
        candle.close     = cV;
        candle.volume    = vV;

        emit newCandle(candle);

        if (tsSec > m_lastEpoch)
            m_lastEpoch = tsSec;
    }
}



QString MarketDataProvider::mapSymbolToFinnhub(const QString& symbol) const
{
    // If the user already provided an exchange-scoped symbol (e.g., "BINANCE:BTCUSDT"), keep it.
    if (symbol.contains(':'))
        return symbol;

    const QString s = symbol.toUpper();
    // Heuristic: treat common crypto quote assets as crypto if no exchange is given.
    const bool looksCrypto =
        s.endsWith("USDT") || s.endsWith("USDC") || s.endsWith("BTC") || s.endsWith("ETH");

    if (looksCrypto) {
        return QStringLiteral("BINANCE:%1").arg(s);
    }
    // Otherwise assume an equity/ETF ticker (e.g., "AAPL")
    return s;
}

void MarketDataProvider::chooseDefaultResolution()
{
    // If your app tracks a timeframe elsewhere, map it here.
    // Minimal default: 1-minute candles.
    m_finnhubResolution = QStringLiteral("1");
    m_resSec = 60;
    // For a daily default instead, uncomment:
    // m_finnhubResolution = QStringLiteral("D");
    // m_resSec = 24 * 3600;
}

QString MarketDataProvider::finnhubCandlePath() const
{
    // Crypto endpoints require exchange-scoped symbol.
    // If symbol is "EXCHANGE:PAIR", assume crypto unless it's clearly an equity venue.
    if (m_finnhubSymbol.contains(':')) {
        const QString ex = m_finnhubSymbol.left(m_finnhubSymbol.indexOf(':')).toUpper();
        if (ex == QLatin1String("BINANCE") || ex == QLatin1String("COINBASE") || ex == QLatin1String("KRAKEN"))
            return QStringLiteral("crypto/candle");
    }
    return QStringLiteral("stock/candle");
}

void MarketDataProvider::setConnected(bool on)
{
    if (m_connected == on)
        return;
    m_connected = on;
    emit connectionStateChanged(m_connected);
}

void MarketDataProvider::applyBackoff(int newMs)
{
    if (newMs <= m_pollMs)
        return;
    m_pollMs = qMin(newMs, 60000);
    if (m_poll.isActive())
        m_poll.setInterval(m_pollMs);
}



