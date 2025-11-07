#pragma once
#include <QObject>
#include <QLoggingCategory>
#include "marketdataprovider.h"
#include "chartmanager.h"
#include "ordermanager.h"
#include "portfoliomanager.h"
#include "storagemanager.h"

Q_DECLARE_LOGGING_CATEGORY(lcApp)

class PaperTraderApp : public QObject {
    Q_OBJECT
public:
    explicit PaperTraderApp(QObject *parent = nullptr);
    void start();
    void stop();

    // Unified feed control for UI toolbar
    void startFeed(MarketDataProvider::FeedMode mode, const QString &symbol = QString());
    void stopFeed();

    MarketDataProvider *dataProvider() const { return m_dataProvider; }
    ChartManager       *chartManager() const { return m_chartManager; }
    OrderManager       *orderManager() const { return m_orderManager; }
    PortfolioManager   *portfolioManager() const { return m_portfolioManager; }
    StorageManager     *storageManager() const { return m_storageManager; }

private:
    MarketDataProvider *m_dataProvider = nullptr;
    ChartManager       *m_chartManager = nullptr;
    OrderManager       *m_orderManager = nullptr;
    PortfolioManager   *m_portfolioManager = nullptr;
    StorageManager     *m_storageManager = nullptr;
};
