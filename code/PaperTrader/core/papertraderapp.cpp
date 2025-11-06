#include "papertraderapp.h"
#include <QDebug>

Q_LOGGING_CATEGORY(lcApp, "app")

PaperTraderApp::PaperTraderApp(QObject *parent)
    : QObject(parent)
{
    m_dataProvider = new MarketDataProvider(this);
    m_chartManager = new ChartManager(this);

    // QObject::connect(m_dataProvider, &MarketDataProvider::newCandle,
    //                  m_chartManager,  &ChartManager::onNewCandle);
    QObject::connect(m_dataProvider, &MarketDataProvider::connectionStateChanged,
                     this, [](bool ok){ qCInfo(lcApp) << (ok ? "Connected" : "Disconnected"); });
}

void PaperTraderApp::start() {
    qCInfo(lcApp) << "Starting PaperTraderApp...";
    m_dataProvider->startFeed(MarketDataProvider::FeedMode::Binance);
}

void PaperTraderApp::stop() {
    qCInfo(lcApp) << "Stopping PaperTraderApp...";
    m_dataProvider->stopFeed();
}
