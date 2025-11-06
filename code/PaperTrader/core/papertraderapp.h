#pragma once
#include <QObject>
#include <QLoggingCategory>
#include "marketdataprovider.h"
#include "chartmanager.h"

Q_DECLARE_LOGGING_CATEGORY(lcApp)

class PaperTraderApp : public QObject {
    Q_OBJECT
public:
    explicit PaperTraderApp(QObject *parent = nullptr);
    void start();
    void stop();

    // Unified feed control for UI toolbar
    void startFeed(MarketDataProvider::FeedMode mode, const QString &symbol = QString());

private:
    MarketDataProvider *m_dataProvider = nullptr;
    ChartManager       *m_chartManager = nullptr;
};
