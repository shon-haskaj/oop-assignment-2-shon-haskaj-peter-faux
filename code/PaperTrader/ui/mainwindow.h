#pragma once
#include <QMainWindow>
#include <QComboBox>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include "core/papertraderapp.h"
#include "chartwidget.h"
#include "core/models/order.h"
#include "core/models/position.h"

class QListWidget;
class QListWidgetItem;
class QTableWidget;
class QToolButton;
class QFrame;
class QIntValidator;
class QDoubleValidator;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onStartFeed();
    void onStopFeed();
    void onFeedModeChanged(int index);
    void onCandleReceived(const Candle &c);
    void onWatchlistToggled(bool expanded);
    void onAddWatchlistSymbol();
    void onWatchlistSymbolActivated(QListWidgetItem *item);
    void onRemoveWatchlistSymbol();
    void onOrderTypeChanged(int index);
    void onPlaceOrder();
    void onCancelSelectedOrder();
    void onOrderSelectionChanged();
    void refreshOrders(const QList<Order> &orders);
    void refreshPortfolio(double cash, double unrealized, const QList<Position> &positions);

private:
    PaperTraderApp *m_app;
    ChartWidget    *m_chart;

    // Toolbar widgets
    QComboBox  *m_feedSelector;
    QLineEdit  *m_symbolEdit;
    QPushButton *m_startButton;
    QPushButton *m_stopButton;
    QLabel     *m_statusLabel;

    // Watchlist UI
    QToolButton *m_watchlistToggle;
    QWidget     *m_watchlistContainer;
    QListWidget *m_watchlistView;
    QLineEdit   *m_watchlistInput;
    QPushButton *m_watchlistAddButton;
    QPushButton *m_watchlistRemoveButton;

    // Order entry UI
    QComboBox   *m_orderTypeCombo;
    QComboBox   *m_orderSideCombo;
    QLineEdit   *m_orderQtyEdit;
    QLineEdit   *m_orderPriceEdit;
    QPushButton *m_placeOrderButton;
    QPushButton *m_cancelOrderButton;
    QTableWidget *m_ordersTable;

    // Portfolio UI
    QLabel      *m_cashLabel;
    QLabel      *m_unrealizedLabel;
    QTableWidget *m_positionsTable;

    QIntValidator    *m_qtyValidator;
    QDoubleValidator *m_priceValidator;

    OrderManager     *m_orderManager = nullptr;
    PortfolioManager *m_portfolioManager = nullptr;
    StorageManager   *m_storage = nullptr;

    QStringList m_watchlist;
    double m_lastPrice = 0.0;
    QString m_lastSymbol;

    MarketDataProvider::FeedMode m_currentMode = MarketDataProvider::FeedMode::Synthetic;

    void setupUi();
    void setupConnections();
    void buildWatchlistPanel(QFrame *panel);
    void buildOrderPanel(QFrame *panel);
    void buildPortfolioPanel(QFrame *panel);
    void populateWatchlist(const QString &selectSymbol = QString());
    void persistWatchlist();
    void persistSettings();
    void loadStateFromStorage();
    void updateOrderPriceVisibility();
    OrderManager::OrderType currentOrderType() const;
};
