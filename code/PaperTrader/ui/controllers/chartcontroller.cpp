#include "chartcontroller.h"

ChartController::ChartController(ChartManager *chartManager, QObject *parent)
    : QObject(parent),
      m_chartManager(chartManager)
{
    if (!m_chartManager)
        return;

    m_mode = m_chartManager->feedMode();

    connect(m_chartManager, &ChartManager::candleReceived,
            this, &ChartController::candleReceived);
    connect(m_chartManager, &ChartManager::connectionStateChanged,
            this, &ChartController::connectionStateChanged);
    connect(m_chartManager, &ChartManager::feedStarted,
            this, &ChartController::feedStarted);
    connect(m_chartManager, &ChartManager::feedStopped,
            this, &ChartController::feedStopped);
    connect(m_chartManager, &ChartManager::lastPriceChanged,
            this, &ChartController::lastPriceChanged);
    connect(m_chartManager, &ChartManager::quoteUpdated,
            this, &ChartController::quoteUpdated);
}

void ChartController::setFeedMode(MarketDataProvider::FeedMode mode)
{
    m_mode = mode;
    if (m_chartManager)
        m_chartManager->setFeedMode(mode);
}

bool ChartController::startFeed(const QString &symbol)
{
    if (!m_chartManager)
        return false;
    return m_chartManager->startFeed(symbol);
}

void ChartController::stopFeed()
{
    if (m_chartManager)
        m_chartManager->stopFeed();
}

QStringList ChartController::loadWatchlist() const
{
    return m_chartManager ? m_chartManager->loadWatchlist() : QStringList{};
}

void ChartController::saveWatchlist(const QStringList &symbols) const
{
    if (m_chartManager)
        m_chartManager->saveWatchlist(symbols);
}

QJsonObject ChartController::loadSettings() const
{
    return m_chartManager ? m_chartManager->loadSettings() : QJsonObject{};
}

void ChartController::saveSettings(const QJsonObject &settings) const
{
    if (m_chartManager)
        m_chartManager->saveSettings(settings);
}

double ChartController::lastPrice() const
{
    return m_chartManager ? m_chartManager->lastPrice() : 0.0;
}

Quote ChartController::lastQuote() const
{
    return m_chartManager ? m_chartManager->lastQuote() : Quote{};
}

QString ChartController::lastSymbol() const
{
    return m_chartManager ? m_chartManager->lastSymbol() : QString{};
}
