#pragma once
#include <QMainWindow>
#include <QComboBox>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include "chartwidget.h"
#include "core/ordermanager.h"
#include "core/models/order.h"
#include "core/models/position.h"
#include "core/models/portfoliosnapshot.h"
#include "core/models/quote.h"
#include "controllers/chartcontroller.h"
#include "controllers/tradingcontroller.h"

class QListWidget;
class QListWidgetItem;
class QTableWidget;
class QToolButton;
class QFrame;
class QIntValidator;
class QDoubleValidator;
class QSplitter;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(ChartController *chartController,
                       TradingController *tradingController,
                       QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onStartFeed();
    void onStopFeed();
    void onFeedModeChanged(int index);
    void onWatchlistToggled(bool expanded);
    void onAddWatchlistSymbol();
    void onWatchlistSymbolActivated(QListWidgetItem *item);
    void onRemoveWatchlistSymbol();
    void onOrderTypeChanged(int index);
    void onOrderSideChanged(int index);
    void onPlaceOrder();
    void onCancelSelectedOrder();
    void onOrderSelectionChanged();
    void refreshOrders(const QList<Order> &orders);
    void refreshPortfolio(const PortfolioSnapshot &snapshot, const QList<Position> &positions);
    void onOrderPanelToggled(bool expanded);
    void onPortfolioToggled(bool expanded);
    void onOrderRejected(const QString &symbol, const QString &errorCode, double rejectedQuantity);
    void onThemeToggled(bool checked);

private:
    ChartWidget *m_chart;

    // Toolbar widgets
    QComboBox  *m_feedSelector;
    QLineEdit  *m_symbolEdit;
    QPushButton *m_startButton;
    QPushButton *m_stopButton;
    QLabel     *m_statusLabel;
    QToolButton *m_themeToggle;

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
    QToolButton *m_orderToggleButton;
    QWidget     *m_orderContainer;

    // Portfolio UI
    QLabel      *m_balanceLabel;
    QLabel      *m_equityLabel;
    QLabel      *m_realizedLabel;
    QLabel      *m_unrealizedLabel;
    QLabel      *m_accountMarginLabel;
    QLabel      *m_orderMarginLabel;
    QLabel      *m_availableFundsLabel;
    QTableWidget *m_positionsTable;
    QToolButton  *m_portfolioToggleButton;
    QWidget      *m_portfolioContent;

    QDoubleValidator *m_qtyValidator;
    QDoubleValidator *m_priceValidator;

    ChartController   *m_chartController = nullptr;
    TradingController *m_tradingController = nullptr;

    QSplitter *m_horizontalSplit = nullptr;
    QSplitter *m_verticalSplit = nullptr;
    QWidget   *m_chartPanelWidget = nullptr;
    QWidget   *m_orderPanelWidget = nullptr;
    QWidget   *m_watchlistPanelWidget = nullptr;
    QWidget   *m_topAreaWidget = nullptr;
    QWidget   *m_portfolioPanelWidget = nullptr;

    QStringList m_watchlist;
    double m_lastPrice = 0.0;
    Quote  m_lastQuote;
    QString m_lastSymbol;
    int m_quantityPrecision = 6;

    int m_savedWatchlistWidth = 260;
    int m_savedOrderWidth = 260;
    int m_savedPortfolioHeight = 240;

    enum class Theme { Dark, Light };
    Theme m_theme = Theme::Dark;

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
    void toggleWatchlistPanel(bool expanded, bool animate);
    void toggleOrderPanel(bool expanded, bool animate);
    void togglePortfolioPanel(bool expanded, bool animate);
    void updateOrderButtonAccent();
    void updateToggleButtonState(QToolButton *button, bool active);
    QString errorCodeToMessage(const QString &code) const;
    void applyTheme(Theme theme);
    QString styleSheetForTheme(Theme theme) const;
};
