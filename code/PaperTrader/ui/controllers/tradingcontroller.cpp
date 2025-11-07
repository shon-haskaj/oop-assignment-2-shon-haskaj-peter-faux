#include "tradingcontroller.h"
#include "core/papertraderapp.h"

TradingController::TradingController(PaperTraderApp *app, QObject *parent)
    : QObject(parent),
      m_app(app)
{
    if (!m_app)
        return;

    bindOrderManager(m_app->orderManager());
    bindPortfolioManager(m_app->portfolioManager());
}

QList<Order> TradingController::orders() const
{
    return m_orderManager ? m_orderManager->orders() : QList<Order>{};
}

PortfolioSnapshot TradingController::snapshot() const
{
    return m_portfolioManager ? m_portfolioManager->snapshot() : PortfolioSnapshot{};
}

QList<Position> TradingController::positions() const
{
    return m_portfolioManager ? m_portfolioManager->positions() : QList<Position>{};
}

OrderManager::OrderPlacementResult TradingController::placeOrder(OrderManager::OrderType type,
                                                                 const QString &symbol,
                                                                 const QString &side,
                                                                 double quantity,
                                                                 double price)
{
    if (!m_orderManager)
        return {};
    return m_orderManager->placeOrder(type, symbol, side, quantity, price);
}

bool TradingController::cancelOrder(int orderId)
{
    return m_orderManager && m_orderManager->cancelOrder(orderId);
}

void TradingController::onLastPriceChanged(const QString &symbol, double price)
{
    if (m_orderManager)
        m_orderManager->setLastPrice(symbol, price);
}

void TradingController::bindOrderManager(OrderManager *manager)
{
    if (manager == m_orderManager)
        return;

    if (m_orderManager)
        disconnect(m_orderManager, nullptr, this, nullptr);

    m_orderManager = manager;
    if (!m_orderManager)
        return;

    connect(m_orderManager, &OrderManager::ordersChanged,
            this, &TradingController::ordersChanged);
    connect(m_orderManager, &OrderManager::orderRejected,
            this, &TradingController::orderRejected);
}

void TradingController::bindPortfolioManager(PortfolioManager *manager)
{
    if (manager == m_portfolioManager)
        return;

    if (m_portfolioManager)
        disconnect(m_portfolioManager, nullptr, this, nullptr);

    m_portfolioManager = manager;
    if (!m_portfolioManager)
        return;

    connect(m_portfolioManager, &PortfolioManager::portfolioChanged,
            this, &TradingController::portfolioChanged);
}
