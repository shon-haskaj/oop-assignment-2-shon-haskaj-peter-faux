#include "chartcontroller.h"
#include "core/storagemanager.h"

ChartController::ChartController(PaperTraderApp *app, QObject *parent)
    : QObject(parent),
      m_app(app)
{
    if (m_app) {
        attachProvider(m_app->dataProvider());
        m_storage = m_app->storageManager();
    }
}

void ChartController::setFeedMode(MarketDataProvider::FeedMode mode)
{
    m_mode = mode;
}

bool ChartController::startFeed(const QString &symbol)
{
    if (!m_app)
        return false;

    const QString trimmed = symbol.trimmed();
    if (trimmed.isEmpty())
        return false;

    m_app->stopFeed();
    m_app->startFeed(m_mode, trimmed);
    attachProvider(m_app->dataProvider());

    m_lastSymbol = trimmed.toUpper();
    m_lastPrice = 0.0;
    emit feedStarted(m_lastSymbol, m_mode);
    emit lastPriceChanged(m_lastSymbol, m_lastPrice);
    return true;
}

void ChartController::stopFeed()
{
    if (!m_app)
        return;

    m_app->stopFeed();
    m_lastPrice = 0.0;
    emit feedStopped();
}

QStringList ChartController::loadWatchlist() const
{
    return m_storage ? m_storage->loadWatchlist() : QStringList{};
}

void ChartController::saveWatchlist(const QStringList &symbols) const
{
    if (m_storage)
        m_storage->saveWatchlist(symbols);
}

QJsonObject ChartController::loadSettings() const
{
    return m_storage ? m_storage->loadSettings() : QJsonObject{};
}

void ChartController::saveSettings(const QJsonObject &settings) const
{
    if (m_storage)
        m_storage->saveSettings(settings);
}

void ChartController::attachProvider(MarketDataProvider *provider)
{
    if (provider == m_provider)
        return;

    if (m_provider) {
        disconnect(m_provider, nullptr, this, nullptr);
    }

    m_provider = provider;
    if (!m_provider)
        return;

    connect(m_provider, &MarketDataProvider::newCandle,
            this, &ChartController::handleCandle);
    connect(m_provider, &MarketDataProvider::connectionStateChanged,
            this, &ChartController::handleConnectionChange);
}

void ChartController::handleCandle(const Candle &c)
{
    m_lastPrice = c.close;
    m_lastSymbol = c.symbol.toUpper();
    emit candleReceived(c);
    emit lastPriceChanged(m_lastSymbol, m_lastPrice);
}

void ChartController::handleConnectionChange(bool connected)
{
    emit connectionStateChanged(connected);
}
