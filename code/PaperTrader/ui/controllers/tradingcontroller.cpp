#include "tradingcontroller.h"
#include "core/papertraderapp.h"

TradingController::TradingController(PaperTraderApp *app, QObject *parent)
    : QObject(parent), m_app(app) {}
