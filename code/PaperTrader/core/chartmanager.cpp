#include "chartmanager.h"

#include <QDateTime>
#include <QMetaType>

#include "storagemanager.h"

ChartManager::ChartManager(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<Quote>("Quote");
}

void ChartManager::setMarketDataProvider(MarketDataProvider *provider)
{
    if (provider == m_provider)
        return;

    if (m_provider)
        disconnect(m_provider, nullptr, this, nullptr);

    m_provider = provider;
    attachProvider(m_provider);
}

void ChartManager::setStorageManager(StorageManager *storage)
{
    m_storage = storage;
}

void ChartManager::setFeedMode(MarketDataProvider::FeedMode mode)
{
    m_mode = mode;
}

bool ChartManager::startFeed(const QString &symbol)
{
    if (!m_provider)
        return false;

    const QString trimmed = symbol.trimmed();
    if (trimmed.isEmpty())
        return false;

    m_provider->stopFeed();
    m_provider->startFeed(m_mode, trimmed);
    attachProvider(m_provider);

    m_lastSymbol = trimmed.toUpper();
    m_lastQuote = {};
    m_lastQuote.symbol = m_lastSymbol;
    m_lastQuote.timestamp = QDateTime::currentDateTimeUtc();
    emit feedStarted(m_lastSymbol, m_mode);
    emit quoteUpdated(m_lastQuote);
    emit lastPriceChanged(m_lastSymbol, m_lastQuote.last);
    return true;
}

void ChartManager::stopFeed()
{
    if (!m_provider)
        return;

    m_provider->stopFeed();
    m_lastQuote.bid = 0.0;
    m_lastQuote.ask = 0.0;
    m_lastQuote.last = 0.0;
    m_lastQuote.timestamp = QDateTime::currentDateTimeUtc();
    emit quoteUpdated(m_lastQuote);
    emit feedStopped();
}

QStringList ChartManager::loadWatchlist() const
{
    return m_storage ? m_storage->loadWatchlist() : QStringList{};
}

void ChartManager::saveWatchlist(const QStringList &symbols) const
{
    if (m_storage)
        m_storage->saveWatchlist(symbols);
}

QJsonObject ChartManager::loadSettings() const
{
    return m_storage ? m_storage->loadSettings() : QJsonObject{};
}

void ChartManager::saveSettings(const QJsonObject &settings) const
{
    if (m_storage)
        m_storage->saveSettings(settings);
}

void ChartManager::handleCandle(const Candle &c)
{
    m_lastSymbol = c.symbol.toUpper();
    m_lastQuote.symbol = m_lastSymbol;
    m_lastQuote.timestamp = c.timestamp.isValid()
            ? c.timestamp
            : QDateTime::currentDateTimeUtc();

    const double baseline = c.close;
    const double spread = std::max(0.01, std::abs(baseline) * 0.0005);
    const double halfSpread = spread / 2.0;

    m_lastQuote.last = baseline;
    m_lastQuote.bid = std::max(0.0, baseline - halfSpread);
    m_lastQuote.ask = baseline + halfSpread;

    emit candleReceived(c);
    emit quoteUpdated(m_lastQuote);
    emit lastPriceChanged(m_lastSymbol, m_lastQuote.last);
}

void ChartManager::handleConnectionChange(bool connected)
{
    emit connectionStateChanged(connected);
}

void ChartManager::attachProvider(MarketDataProvider *provider)
{
    if (!provider)
        return;

    connect(provider, &MarketDataProvider::newCandle,
            this, &ChartManager::handleCandle, Qt::UniqueConnection);
    connect(provider, &MarketDataProvider::connectionStateChanged,
            this, &ChartManager::handleConnectionChange, Qt::UniqueConnection);
}
