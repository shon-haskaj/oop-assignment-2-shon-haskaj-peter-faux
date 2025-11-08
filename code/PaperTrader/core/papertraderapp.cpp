#include "papertraderapp.h"
#include <QDebug>

Q_LOGGING_CATEGORY(lcApp, "app")

PaperTraderApp::PaperTraderApp(QObject *parent)
    : QObject(parent)
{
    m_dataProvider = new MarketDataProvider(this);
    m_chartManager = new ChartManager(this);
    m_orderManager = new OrderManager(this);
    m_portfolioManager = new PortfolioManager(this);
    m_storageManager = new StorageManager(this);
    m_executionSimulator = new ExecutionSimulator(this);

    m_chartManager->setMarketDataProvider(m_dataProvider);
    m_chartManager->setStorageManager(m_storageManager);

    m_orderManager->setPortfolioManager(m_portfolioManager);
    m_executionSimulator->setOrderManager(m_orderManager);
    m_executionSimulator->setPortfolioManager(m_portfolioManager);

    QObject::connect(m_dataProvider, &MarketDataProvider::connectionStateChanged,
                     this, [](bool ok){ qCInfo(lcApp) << (ok ? "Connected" : "Disconnected"); });

    QObject::connect(m_dataProvider, &MarketDataProvider::newCandle,
                     m_portfolioManager, &PortfolioManager::onCandle);
    QObject::connect(m_dataProvider, &MarketDataProvider::newCandle,
                     m_executionSimulator, &ExecutionSimulator::onCandle);

    QObject::connect(m_orderManager, &OrderManager::orderFilled,
                     m_portfolioManager, &PortfolioManager::applyFill);
    QObject::connect(m_orderManager, &OrderManager::ordersChanged,
                     m_portfolioManager, &PortfolioManager::onOrdersUpdated);
}

void PaperTraderApp::start() {
    qCInfo(lcApp) << "Starting PaperTraderApp...";
    m_dataProvider->startFeed(MarketDataProvider::FeedMode::Binance);
}

void PaperTraderApp::stop() {
    qCInfo(lcApp) << "Stopping PaperTraderApp...";
    m_dataProvider->stopFeed();
}

void PaperTraderApp::startFeed(MarketDataProvider::FeedMode mode, const QString &symbol)
{
    if (!m_dataProvider)
        return;
    m_dataProvider->startFeed(mode, symbol);
}

void PaperTraderApp::stopFeed()
{
    if (!m_dataProvider)
        return;
    m_dataProvider->stopFeed();
}
