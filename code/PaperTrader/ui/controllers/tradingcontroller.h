#pragma once
#include <QObject>
class PaperTraderApp;

class TradingController : public QObject {
    Q_OBJECT
public:
    explicit TradingController(PaperTraderApp *app, QObject *parent = nullptr);

private:
    PaperTraderApp *m_app;
};
