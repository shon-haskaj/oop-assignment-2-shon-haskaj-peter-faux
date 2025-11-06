#include <QtTest/QtTest>
#include <cmath>

#include "core/ordermanager.h"
#include "core/portfoliomanager.h"
#include "core/models/order.h"
#include "core/models/position.h"
#include "core/models/portfoliosnapshot.h"

// Helper macro for readable fuzzy comparisons in assertions.
#define VERIFY_NEAR(actual, expected, epsilon) \
    QVERIFY2(std::fabs((actual) - (expected)) <= (epsilon), \
             qPrintable(QStringLiteral("Expected %1 ≈ %2 (±%3)") \
                            .arg(QString::number(actual, 'f', 6)) \
                            .arg(QString::number(expected, 'f', 6)) \
                            .arg(QString::number(epsilon, 'f', 6))))

class TradingLogicTests : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();

    void test_marketBuyLong();
    void test_marketSellShort();
    void test_longSideFlip();
    void test_shortSideFlip();
    void test_buyRejectedForFunds();
    void test_shortRejectedForMargin();
    void test_limitBuyRejected();
    void test_limitShortRejected();
    void test_cancelReleasesOrderMargin();
    void test_partialFillReducesOrderMargin();
    void test_feeHandlingOnClose();
};

void TradingLogicTests::initTestCase()
{
    qRegisterMetaType<Order>("Order");
    qRegisterMetaType<QList<Order>>("QList<Order>");
    qRegisterMetaType<PortfolioSnapshot>("PortfolioSnapshot");
    qRegisterMetaType<QList<Position>>("QList<Position>");
}

static void connectManagers(OrderManager &om, PortfolioManager &pm)
{
    QObject::connect(&om, &OrderManager::orderFilled,
                     &pm, &PortfolioManager::applyFill);
    QObject::connect(&om, &OrderManager::ordersChanged,
                     &pm, &PortfolioManager::onOrdersUpdated);
}

void TradingLogicTests::test_marketBuyLong()
{
    // Buying 1.25 units at 20,000 should reduce cash by notional plus fees,
    // open a long position, and leave no margin reserved.
    PortfolioManager pm;
    OrderManager om;
    om.setPortfolioManager(&pm);
    connectManagers(om, pm);

    om.setLastPrice("BTCUSDT", 20000.0);
    auto result = om.placeOrder(OrderManager::OrderType::Market,
                                "BTCUSDT", "BUY", 1.25, 20000.0);

    QVERIFY(result.accepted);
    QVERIFY(!result.partial);

    const auto snapshot = pm.snapshot();
    VERIFY_NEAR(snapshot.accountBalance, 74990.0, 1e-3); // 100000 - 1.25*20000 - fees
    VERIFY_NEAR(snapshot.availableFunds, 74990.0, 1e-3);
    VERIFY_NEAR(snapshot.accountMargin, 0.0, 1e-6);
    VERIFY_NEAR(snapshot.orderMargin, 0.0, 1e-6);

    const auto positions = pm.positions();
    QCOMPARE(positions.size(), 1);
    const Position &pos = positions.first();
    QCOMPARE(pos.symbol, QStringLiteral("BTCUSDT"));
    VERIFY_NEAR(pos.qty, 1.25, 1e-9);
    VERIFY_NEAR(pos.avgPx, 20000.0, 1e-6);
    VERIFY_NEAR(pos.shortCollateral, 0.0, 1e-6);
}

void TradingLogicTests::test_marketSellShort()
{
    // Opening a short keeps cash untouched apart from the fee and reserves margin.
    PortfolioManager pm;
    OrderManager om;
    om.setPortfolioManager(&pm);
    connectManagers(om, pm);

    om.setLastPrice("ABC", 100.0);
    auto result = om.placeOrder(OrderManager::OrderType::Market,
                                "ABC", "SELL", 0.5, 100.0);

    QVERIFY(result.accepted);
    QVERIFY(!result.partial);

    const auto snapshot = pm.snapshot();
    VERIFY_NEAR(snapshot.accountBalance, 99999.98, 1e-6); // 100000 - fee
    VERIFY_NEAR(snapshot.accountMargin, 25.0, 1e-6);      // |0.5| * 100 * 0.5
    VERIFY_NEAR(snapshot.availableFunds, 99974.98, 1e-6);

    const auto positions = pm.positions();
    QCOMPARE(positions.size(), 1);
    const Position &pos = positions.first();
    VERIFY_NEAR(pos.qty, -0.5, 1e-9);
    VERIFY_NEAR(pos.avgPx, 100.0, 1e-6);
    VERIFY_NEAR(pos.shortCollateral, 50.0, 1e-6);
}

void TradingLogicTests::test_longSideFlip()
{
    // Buy 60 @100 then sell 90 @110 should realise profit on 60 and open a 30 unit short.
    PortfolioManager pm;
    OrderManager om;
    om.setPortfolioManager(&pm);
    connectManagers(om, pm);

    om.setLastPrice("XYZ", 100.0);
    om.placeOrder(OrderManager::OrderType::Market, "XYZ", "BUY", 60.0, 100.0);

    om.setLastPrice("XYZ", 110.0);
    auto flip = om.placeOrder(OrderManager::OrderType::Market, "XYZ", "SELL", 90.0, 110.0);
    QVERIFY(flip.accepted);
    QVERIFY(!flip.partial);

    const auto snapshot = pm.snapshot();
    VERIFY_NEAR(snapshot.accountBalance, 100593.64, 1e-2);
    VERIFY_NEAR(snapshot.accountMargin, 1650.0, 1e-2); // 30 short * 110 * 0.5
    VERIFY_NEAR(snapshot.realizedPnL, 597.36, 1e-2);

    const auto positions = pm.positions();
    QCOMPARE(positions.size(), 1);
    const Position &pos = positions.first();
    VERIFY_NEAR(pos.qty, -30.0, 1e-6);
    VERIFY_NEAR(pos.avgPx, 110.0, 1e-6);
    VERIFY_NEAR(pos.shortCollateral, 3300.0, 1e-6);
}

void TradingLogicTests::test_shortSideFlip()
{
    // Short 50 @75, cover 20 @70, then flip long by buying 80 @72.
    PortfolioManager pm;
    OrderManager om;
    om.setPortfolioManager(&pm);
    connectManagers(om, pm);

    om.setLastPrice("QQQ", 75.0);
    om.placeOrder(OrderManager::OrderType::Market, "QQQ", "SELL", 50.0, 75.0);

    om.setLastPrice("QQQ", 70.0);
    om.placeOrder(OrderManager::OrderType::Market, "QQQ", "BUY", 20.0, 70.0);

    om.setLastPrice("QQQ", 72.0);
    auto flip = om.placeOrder(OrderManager::OrderType::Market, "QQQ", "BUY", 80.0, 72.0);
    QVERIFY(flip.accepted);
    QVERIFY(!flip.partial);

    const auto snapshot = pm.snapshot();
    VERIFY_NEAR(snapshot.accountBalance, 96585.636, 1e-3);
    VERIFY_NEAR(snapshot.realizedPnL, 188.576, 1e-3);
    VERIFY_NEAR(snapshot.accountMargin, 0.0, 1e-6); // fully long afterwards

    const auto positions = pm.positions();
    QCOMPARE(positions.size(), 1);
    const Position &pos = positions.first();
    VERIFY_NEAR(pos.qty, 50.0, 1e-6);
    VERIFY_NEAR(pos.avgPx, 72.0, 1e-6);
    VERIFY_NEAR(pos.shortCollateral, 0.0, 1e-6);
}

void TradingLogicTests::test_buyRejectedForFunds()
{
    PortfolioManager pm;
    OrderManager om;
    om.setPortfolioManager(&pm);

    om.setLastPrice("FUNDS", 500000.0);
    auto result = om.placeOrder(OrderManager::OrderType::Market,
                                "FUNDS", "BUY", 5.0, 500000.0);
    QVERIFY(!result.accepted);
    QCOMPARE(result.errorCode, QStringLiteral("ERR_INSUFFICIENT_FUNDS"));
}

void TradingLogicTests::test_shortRejectedForMargin()
{
    PortfolioManager pm;
    OrderManager om;
    om.setPortfolioManager(&pm);

    om.setLastPrice("MARGIN", 1000.0);
    auto result = om.placeOrder(OrderManager::OrderType::Market,
                                "MARGIN", "SELL", 400.0, 1000.0);
    QVERIFY(!result.accepted);
    QCOMPARE(result.errorCode, QStringLiteral("ERR_INSUFFICIENT_MARGIN"));
}

void TradingLogicTests::test_limitBuyRejected()
{
    PortfolioManager pm;
    OrderManager om;
    om.setPortfolioManager(&pm);

    auto result = om.placeOrder(OrderManager::OrderType::Limit,
                                "LIMITBUY", "BUY", 2.0, 75000.0);
    QVERIFY(!result.accepted);
    QCOMPARE(result.errorCode, QStringLiteral("ERR_INSUFFICIENT_FUNDS"));
}

void TradingLogicTests::test_limitShortRejected()
{
    PortfolioManager pm;
    OrderManager om;
    om.setPortfolioManager(&pm);

    auto result = om.placeOrder(OrderManager::OrderType::Limit,
                                "LIMITSELL", "SELL", 500.0, 800.0);
    QVERIFY(!result.accepted);
    QCOMPARE(result.errorCode, QStringLiteral("ERR_INSUFFICIENT_MARGIN"));
}

void TradingLogicTests::test_cancelReleasesOrderMargin()
{
    PortfolioManager pm;
    OrderManager om;
    om.setPortfolioManager(&pm);
    connectManagers(om, pm);

    om.setLastPrice("ORDER", 1000.0);
    auto result = om.placeOrder(OrderManager::OrderType::Limit,
                                "ORDER", "BUY", 10.0, 1000.0);
    QVERIFY(result.accepted);

    auto snapshot = pm.snapshot();
    VERIFY_NEAR(snapshot.orderMargin, 10004.0, 1e-3); // 10 * 1000 * (1 + fee rate)

    QVERIFY(om.cancelOrder(result.order.id));
    snapshot = pm.snapshot();
    VERIFY_NEAR(snapshot.orderMargin, 0.0, 1e-6);
}

void TradingLogicTests::test_partialFillReducesOrderMargin()
{
    PortfolioManager pm;
    OrderManager om;
    om.setPortfolioManager(&pm);
    connectManagers(om, pm);

    om.setLastPrice("PARTIAL", 100.0);
    auto result = om.placeOrder(OrderManager::OrderType::Limit,
                                "PARTIAL", "BUY", 10.0, 100.0);
    QVERIFY(result.accepted);

    // Simulate a fill of 4 units so only 6 remain working on the order book.
    Order fill;
    fill.symbol = "PARTIAL";
    fill.side = "BUY";
    fill.price = 100.0;
    fill.filledPrice = 100.0;
    fill.filledQuantity = 4.0;
    fill.quantity = 10.0;
    fill.fee = pm.estimateFee(100.0, 4.0);
    pm.applyFill(fill);

    Order remaining = result.order;
    remaining.quantity = 6.0;
    remaining.status = QStringLiteral("Open");
    pm.onOrdersUpdated({remaining});

    const auto snapshot = pm.snapshot();
    VERIFY_NEAR(snapshot.orderMargin, 600.24, 1e-2); // 6 * 100 * (1 + fee rate)
}

void TradingLogicTests::test_feeHandlingOnClose()
{
    PortfolioManager pm;
    OrderManager om;
    om.setPortfolioManager(&pm);
    connectManagers(om, pm);

    om.setLastPrice("FEE", 100.0);
    om.placeOrder(OrderManager::OrderType::Market, "FEE", "BUY", 10.0, 100.0);

    om.setLastPrice("FEE", 80.0);
    auto exit = om.placeOrder(OrderManager::OrderType::Market, "FEE", "SELL", 10.0, 80.0);
    QVERIFY(exit.accepted);
    QVERIFY(!exit.partial);

    const auto snapshot = pm.snapshot();
    VERIFY_NEAR(snapshot.accountBalance, 99792.8, 1e-3); // 100000 - 1000 - 4 + 800 - 3.2
    VERIFY_NEAR(snapshot.realizedPnL, -203.2, 1e-3);
    QVERIFY(pm.positions().isEmpty());
}

QTEST_MAIN(TradingLogicTests)
#include "test_tradinglogic.moc"
