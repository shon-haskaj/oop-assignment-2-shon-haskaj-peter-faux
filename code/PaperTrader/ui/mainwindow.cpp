#include "mainwindow.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QStatusBar>
#include <QSplitter>
#include <QToolButton>
#include <QListWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QGridLayout>
#include <QLabel>
#include <QDoubleValidator>
#include <QLocale>
#include <QJsonObject>
#include <QItemSelectionModel>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_app(new PaperTraderApp(this)),
      m_chart(new ChartWidget(this))
{
    m_orderManager = m_app->orderManager();
    m_portfolioManager = m_app->portfolioManager();
    m_storage = m_app->storageManager();

    setupUi();
    setupConnections();
    loadStateFromStorage();

    m_statusLabel->setText("🔴 Disconnected");
}

MainWindow::~MainWindow()
{
    persistSettings();
    persistWatchlist();
}

void MainWindow::setupUi()
{
    QWidget *central = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(10);

    QFrame *toolbar = new QFrame(this);
    toolbar->setObjectName("toolbar");
    toolbar->setFrameShape(QFrame::NoFrame);
    QHBoxLayout *toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(16, 10, 16, 10);
    toolbarLayout->setSpacing(10);

    m_feedSelector = new QComboBox(this);
    m_feedSelector->addItems({"Synthetic", "Binance"});

    m_symbolEdit = new QLineEdit(this);
    m_symbolEdit->setPlaceholderText("Symbol (e.g. btcusdt)");

    m_startButton = new QPushButton("Start Feed", this);
    m_stopButton  = new QPushButton("Stop Feed", this);
    m_statusLabel = new QLabel("", this);
    m_statusLabel->setMinimumWidth(120);

    toolbarLayout->addWidget(new QLabel("Feed:", this));
    toolbarLayout->addWidget(m_feedSelector);
    toolbarLayout->addWidget(new QLabel("Symbol:", this));
    toolbarLayout->addWidget(m_symbolEdit, 1);
    toolbarLayout->addWidget(m_startButton);
    toolbarLayout->addWidget(m_stopButton);
    toolbarLayout->addWidget(m_statusLabel);

    // Build a TradingView-inspired layout: central chart, right-side drawers, bottom portfolio dock.
    QSplitter *verticalSplit = new QSplitter(Qt::Vertical, this);
    verticalSplit->setChildrenCollapsible(false);
    verticalSplit->setHandleWidth(2);

    QWidget *topArea = new QWidget(verticalSplit);
    QVBoxLayout *topLayout = new QVBoxLayout(topArea);
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->setSpacing(8);

    QSplitter *horizontalSplit = new QSplitter(Qt::Horizontal, topArea);
    horizontalSplit->setChildrenCollapsible(false);
    horizontalSplit->setHandleWidth(2);

    QFrame *chartPanel = new QFrame(horizontalSplit);
    chartPanel->setObjectName("chartPanel");
    QVBoxLayout *chartLayout = new QVBoxLayout(chartPanel);
    chartLayout->setContentsMargins(0, 0, 0, 0);
    chartLayout->addWidget(m_chart);

    QFrame *orderPanel = new QFrame(horizontalSplit);
    orderPanel->setObjectName("orderPanel");
    buildOrderPanel(orderPanel);

    QFrame *watchlistPanel = new QFrame(horizontalSplit);
    watchlistPanel->setObjectName("watchlistPanel");
    buildWatchlistPanel(watchlistPanel);

    horizontalSplit->addWidget(chartPanel);
    horizontalSplit->addWidget(orderPanel);
    horizontalSplit->addWidget(watchlistPanel);
    horizontalSplit->setStretchFactor(0, 1);
    horizontalSplit->setStretchFactor(1, 0);
    horizontalSplit->setStretchFactor(2, 0);

    topLayout->addWidget(horizontalSplit);

    QFrame *portfolioPanel = new QFrame(verticalSplit);
    portfolioPanel->setObjectName("portfolioPanel");
    buildPortfolioPanel(portfolioPanel);

    verticalSplit->addWidget(topArea);
    verticalSplit->addWidget(portfolioPanel);
    verticalSplit->setStretchFactor(0, 1);
    verticalSplit->setStretchFactor(1, 0);

    mainLayout->addWidget(toolbar);
    mainLayout->addWidget(verticalSplit, 1);

    onWatchlistToggled(m_watchlistToggle->isChecked());
    onOrderPanelToggled(m_orderToggleButton->isChecked());
    onPortfolioToggled(m_portfolioToggleButton->isChecked());

    setCentralWidget(central);
    setWindowTitle("PaperTrader - Market Feed Viewer");
    resize(1280, 720);
    statusBar()->setVisible(false);

    setStyleSheet(
        "QWidget { color: #e8ecf1; background-color: #0f111a; font-family: 'Inter', 'Segoe UI', sans-serif; }"
        "QFrame#toolbar { background-color: #1c2030; border-radius: 10px; }"
        "QFrame#watchlistPanel, QFrame#orderPanel, QFrame#portfolioPanel {"
        " background-color: #161a26; border: 1px solid #252a3d; border-radius: 10px; }"
        "QLineEdit, QComboBox { background-color: #111522; border: 1px solid #2c3249;"
        " border-radius: 6px; padding: 4px 8px; }"
        "QPushButton { background-color: #2151f2; border: none; border-radius: 6px;"
        " padding: 6px 14px; color: white; font-weight: 600; }"
        "QPushButton:hover { background-color: #3a65ff; }"
        "QLabel#sectionTitle { font-weight: 600; font-size: 14px; color: #a7b5d8; }"
        "QToolButton#watchlistToggle, QToolButton#orderToggle, QToolButton#portfolioToggle {"
        " background-color: transparent; border: none; color: #a7b5d8; }"
        "QListWidget { background-color: #111522; border: 1px solid #2c3249;"
        " border-radius: 6px; }"
        "QTableWidget { background-color: #111522; border: 1px solid #2c3249;"
        " border-radius: 6px; gridline-color: #1e2334; }"
        "QHeaderView::section { background-color: #161c2c; border: none; color: #aab6d6; }"
        "QLabel#metricValue { font-size: 18px; font-weight: 600; }"
    );
}

void MainWindow::buildWatchlistPanel(QFrame *panel)
{
    QVBoxLayout *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    QHBoxLayout *header = new QHBoxLayout();
    header->setContentsMargins(0, 0, 0, 0);
    QLabel *title = new QLabel("Watchlist", panel);
    title->setObjectName("sectionTitle");
    header->addWidget(title);
    header->addStretch(1);
    // Edge toggle keeps the drawer accessible while collapsed.
    m_watchlistToggle = new QToolButton(panel);
    m_watchlistToggle->setObjectName("watchlistToggle");
    m_watchlistToggle->setCheckable(true);
    m_watchlistToggle->setChecked(true);
    m_watchlistToggle->setArrowType(Qt::RightArrow);
    m_watchlistToggle->setToolTip(tr("Collapse watchlist"));
    header->addWidget(m_watchlistToggle, 0, Qt::AlignRight);
    layout->addLayout(header);

    m_watchlistContainer = new QWidget(panel);
    QVBoxLayout *containerLayout = new QVBoxLayout(m_watchlistContainer);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->setSpacing(8);

    QHBoxLayout *inputLayout = new QHBoxLayout();
    inputLayout->setContentsMargins(0, 0, 0, 0);
    inputLayout->setSpacing(6);
    m_watchlistInput = new QLineEdit(panel);
    m_watchlistInput->setPlaceholderText("Add symbol");
    m_watchlistAddButton = new QPushButton("Add", panel);
    m_watchlistRemoveButton = new QPushButton("Remove", panel);
    m_watchlistRemoveButton->setStyleSheet("QPushButton { background-color: #2b3249; }"
                                          "QPushButton:hover { background-color: #39405b; }");
    inputLayout->addWidget(m_watchlistInput, 1);
    inputLayout->addWidget(m_watchlistAddButton);
    inputLayout->addWidget(m_watchlistRemoveButton);

    m_watchlistView = new QListWidget(panel);
    m_watchlistView->setAlternatingRowColors(true);
    m_watchlistView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_watchlistView->setMinimumWidth(180);

    containerLayout->addLayout(inputLayout);
    containerLayout->addWidget(m_watchlistView, 1);

    layout->addWidget(m_watchlistContainer);
}

void MainWindow::buildOrderPanel(QFrame *panel)
{
    QVBoxLayout *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    QHBoxLayout *header = new QHBoxLayout();
    header->setContentsMargins(0, 0, 0, 0);
    QLabel *title = new QLabel("Order Entry", panel);
    title->setObjectName("sectionTitle");
    header->addWidget(title);
    header->addStretch(1);
    // Collapsible order ticket mirrors the TradingView drawer interaction.
    m_orderToggleButton = new QToolButton(panel);
    m_orderToggleButton->setObjectName("orderToggle");
    m_orderToggleButton->setCheckable(true);
    m_orderToggleButton->setChecked(true);
    m_orderToggleButton->setArrowType(Qt::RightArrow);
    m_orderToggleButton->setToolTip(tr("Collapse order ticket"));
    header->addWidget(m_orderToggleButton, 0, Qt::AlignRight);
    layout->addLayout(header);

    m_orderContainer = new QWidget(panel);
    QVBoxLayout *containerLayout = new QVBoxLayout(m_orderContainer);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->setSpacing(10);

    QGridLayout *form = new QGridLayout();
    form->setHorizontalSpacing(8);
    form->setVerticalSpacing(6);

    m_orderTypeCombo = new QComboBox(panel);
    m_orderTypeCombo->addItems({"Market", "Limit"});

    m_orderSideCombo = new QComboBox(panel);
    m_orderSideCombo->addItems({"Buy", "Sell"});

    m_orderQtyEdit = new QLineEdit(panel);
    m_orderQtyEdit->setPlaceholderText("Quantity");
    m_qtyValidator = new QDoubleValidator(0.000001, 1e9, m_quantityPrecision, this);
    m_qtyValidator->setNotation(QDoubleValidator::StandardNotation);
    m_orderQtyEdit->setValidator(m_qtyValidator);

    m_orderPriceEdit = new QLineEdit(panel);
    m_orderPriceEdit->setPlaceholderText("Price");
    m_priceValidator = new QDoubleValidator(0.0, 1e9, 8, this);
    m_priceValidator->setNotation(QDoubleValidator::StandardNotation);
    m_orderPriceEdit->setValidator(m_priceValidator);

    form->addWidget(new QLabel("Type", panel), 0, 0);
    form->addWidget(m_orderTypeCombo, 0, 1);
    form->addWidget(new QLabel("Side", panel), 0, 2);
    form->addWidget(m_orderSideCombo, 0, 3);
    form->addWidget(new QLabel("Qty", panel), 1, 0);
    form->addWidget(m_orderQtyEdit, 1, 1);
    form->addWidget(new QLabel("Price", panel), 1, 2);
    form->addWidget(m_orderPriceEdit, 1, 3);

    containerLayout->addLayout(form);

    QHBoxLayout *buttonRow = new QHBoxLayout();
    buttonRow->setSpacing(8);
    m_placeOrderButton = new QPushButton("Place Order", panel);
    m_cancelOrderButton = new QPushButton("Cancel Selected", panel);
    m_cancelOrderButton->setEnabled(false);
    buttonRow->addWidget(m_placeOrderButton, 1);
    buttonRow->addWidget(m_cancelOrderButton, 1);
    containerLayout->addLayout(buttonRow);

    QLabel *ordersHeader = new QLabel("Orders", panel);
    ordersHeader->setObjectName("sectionTitle");
    containerLayout->addWidget(ordersHeader);

    m_ordersTable = new QTableWidget(panel);
    m_ordersTable->setColumnCount(7);
    m_ordersTable->setHorizontalHeaderLabels({"ID", "Symbol", "Side", "Type", "Qty", "Price", "Status"});
    m_ordersTable->horizontalHeader()->setStretchLastSection(true);
    m_ordersTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_ordersTable->verticalHeader()->setVisible(false);
    m_ordersTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_ordersTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_ordersTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    containerLayout->addWidget(m_ordersTable, 1);

    layout->addWidget(m_orderContainer);

    updateOrderPriceVisibility();
}

void MainWindow::buildPortfolioPanel(QFrame *panel)
{
    QVBoxLayout *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    QLabel *title = new QLabel("Portfolio", panel);
    title->setObjectName("sectionTitle");
    layout->addWidget(title);

    m_portfolioContent = new QWidget(panel);
    QVBoxLayout *contentLayout = new QVBoxLayout(m_portfolioContent);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(12);

    QGridLayout *metrics = new QGridLayout();
    metrics->setHorizontalSpacing(18);
    metrics->setVerticalSpacing(6);

    auto addMetric = [&](int row, int column, const QString &labelText, QLabel *&valueLabel) {
        QLabel *label = new QLabel(labelText, panel);
        valueLabel = new QLabel("$0.00", panel);
        valueLabel->setObjectName("metricValue");
        metrics->addWidget(label, row, column * 2);
        metrics->addWidget(valueLabel, row, column * 2 + 1);
    };

    addMetric(0, 0, tr("Account Balance"), m_balanceLabel);
    addMetric(0, 1, tr("Equity"), m_equityLabel);
    addMetric(0, 2, tr("Realized P&L"), m_realizedLabel);
    addMetric(1, 0, tr("Unrealized P&L"), m_unrealizedLabel);
    addMetric(1, 1, tr("Account Margin"), m_accountMarginLabel);
    addMetric(1, 2, tr("Order Margin"), m_orderMarginLabel);
    addMetric(2, 0, tr("Available Funds"), m_availableFundsLabel);

    contentLayout->addLayout(metrics);

    m_positionsTable = new QTableWidget(panel);
    m_positionsTable->setColumnCount(5);
    m_positionsTable->setHorizontalHeaderLabels({"Symbol", "Qty", "Avg Price", "Last", "Unrealized"});
    m_positionsTable->horizontalHeader()->setStretchLastSection(true);
    m_positionsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_positionsTable->verticalHeader()->setVisible(false);
    m_positionsTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_positionsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    contentLayout->addWidget(m_positionsTable, 1);

    layout->addWidget(m_portfolioContent, 1);

    QHBoxLayout *footer = new QHBoxLayout();
    footer->setContentsMargins(0, 0, 0, 0);
    footer->addStretch(1);
    // Bottom-right toggle emulates TradingView's collapsible account bar.
    m_portfolioToggleButton = new QToolButton(panel);
    m_portfolioToggleButton->setObjectName("portfolioToggle");
    m_portfolioToggleButton->setCheckable(true);
    m_portfolioToggleButton->setChecked(true);
    m_portfolioToggleButton->setArrowType(Qt::DownArrow);
    m_portfolioToggleButton->setToolTip(tr("Collapse portfolio panel"));
    footer->addWidget(m_portfolioToggleButton);
    layout->addLayout(footer);
}

void MainWindow::setupConnections()
{
    connect(m_feedSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onFeedModeChanged);

    connect(m_startButton, &QPushButton::clicked, this, &MainWindow::onStartFeed);
    connect(m_stopButton, &QPushButton::clicked, this, &MainWindow::onStopFeed);

    connect(m_watchlistToggle, &QToolButton::toggled,
            this, &MainWindow::onWatchlistToggled);
    connect(m_orderToggleButton, &QToolButton::toggled,
            this, &MainWindow::onOrderPanelToggled);
    connect(m_portfolioToggleButton, &QToolButton::toggled,
            this, &MainWindow::onPortfolioToggled);
    connect(m_watchlistAddButton, &QPushButton::clicked,
            this, &MainWindow::onAddWatchlistSymbol);
    connect(m_watchlistRemoveButton, &QPushButton::clicked,
            this, &MainWindow::onRemoveWatchlistSymbol);

    connect(m_watchlistView, &QListWidget::itemDoubleClicked,
            this, &MainWindow::onWatchlistSymbolActivated);
    connect(m_watchlistView, &QListWidget::itemActivated,
            this, &MainWindow::onWatchlistSymbolActivated);
    connect(m_watchlistView, &QListWidget::itemSelectionChanged, this, [this]() {
        m_watchlistRemoveButton->setEnabled(m_watchlistView->currentItem());
    });

    connect(m_orderTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onOrderTypeChanged);
    connect(m_placeOrderButton, &QPushButton::clicked,
            this, &MainWindow::onPlaceOrder);
    connect(m_cancelOrderButton, &QPushButton::clicked,
            this, &MainWindow::onCancelSelectedOrder);

    if (auto *selection = m_ordersTable->selectionModel()) {
        connect(selection, &QItemSelectionModel::selectionChanged,
                this, &MainWindow::onOrderSelectionChanged);
    }

    if (auto provider = m_app->dataProvider()) {
        connect(provider, &MarketDataProvider::newCandle,
                m_chart, &ChartWidget::appendCandle);
        connect(provider, &MarketDataProvider::newCandle,
                this, &MainWindow::onCandleReceived);
        connect(provider, &MarketDataProvider::connectionStateChanged,
                this, [this](bool connected) {
                    m_statusLabel->setText(connected ? "🟢 Connected" : "🔴 Disconnected");
                });
    }

    if (m_orderManager) {
        connect(m_orderManager, &OrderManager::ordersChanged,
                this, &MainWindow::refreshOrders);
        refreshOrders(m_orderManager->orders());
    }

    if (m_portfolioManager) {
        connect(m_portfolioManager, &PortfolioManager::portfolioChanged,
                this, &MainWindow::refreshPortfolio);
        refreshPortfolio(m_portfolioManager->snapshot(),
                         m_portfolioManager->positions());
    }
}

void MainWindow::onFeedModeChanged(int index)
{
    m_currentMode = (index == 1)
            ? MarketDataProvider::FeedMode::Binance
            : MarketDataProvider::FeedMode::Synthetic;
    persistSettings();
}

void MainWindow::onStartFeed()
{
    QString symbol = m_symbolEdit->text().trimmed();
    if (symbol.isEmpty()) {
        symbol = QStringLiteral("btcusdt");
        m_symbolEdit->setText(symbol);
    }

    if (!m_app)
        return;

    m_app->stopFeed();
    m_app->startFeed(m_currentMode, symbol);

    m_lastSymbol = symbol.toUpper();
    setWindowTitle(QString("PaperTrader - %1 (%2)")
                       .arg(m_lastSymbol)
                       .arg(m_feedSelector->currentText()));
    persistSettings();
}

void MainWindow::onStopFeed()
{
    if (m_app)
        m_app->stopFeed();

    m_chart->clearCandles();
    m_statusLabel->setText("🔴 Disconnected");
    setWindowTitle("PaperTrader - Market Feed Viewer");
}

void MainWindow::onCandleReceived(const Candle &c)
{
    m_lastPrice = c.close;
    m_lastSymbol = c.symbol.toUpper();
    if (m_orderManager) {
        m_orderManager->setLastPrice(m_lastSymbol, m_lastPrice);
    }
}

void MainWindow::onWatchlistToggled(bool expanded)
{
    m_watchlistToggle->setArrowType(expanded ? Qt::RightArrow : Qt::LeftArrow);
    m_watchlistToggle->setToolTip(expanded ? tr("Collapse watchlist")
                                         : tr("Expand watchlist"));
    m_watchlistContainer->setVisible(expanded);
}

void MainWindow::onOrderPanelToggled(bool expanded)
{
    m_orderToggleButton->setArrowType(expanded ? Qt::RightArrow : Qt::LeftArrow);
    m_orderToggleButton->setToolTip(expanded ? tr("Collapse order ticket")
                                             : tr("Expand order ticket"));
    m_orderContainer->setVisible(expanded);
}

void MainWindow::onPortfolioToggled(bool expanded)
{
    m_portfolioToggleButton->setArrowType(expanded ? Qt::DownArrow : Qt::UpArrow);
    m_portfolioToggleButton->setToolTip(expanded ? tr("Collapse portfolio panel")
                                                 : tr("Expand portfolio panel"));
    m_portfolioContent->setVisible(expanded);
}

void MainWindow::onAddWatchlistSymbol()
{
    QString symbol = m_watchlistInput->text().trimmed().toUpper();
    if (symbol.isEmpty())
        return;
    if (!m_watchlist.contains(symbol)) {
        m_watchlist.append(symbol);
        populateWatchlist(symbol);
        persistWatchlist();
    } else {
        populateWatchlist(symbol);
    }
    m_watchlistInput->clear();
}

void MainWindow::onRemoveWatchlistSymbol()
{
    auto *item = m_watchlistView->currentItem();
    if (!item)
        return;
    const QString symbol = item->text();
    m_watchlist.removeAll(symbol);
    populateWatchlist();
    persistWatchlist();
}

void MainWindow::onWatchlistSymbolActivated(QListWidgetItem *item)
{
    if (!item)
        return;
    const QString symbol = item->text();
    m_symbolEdit->setText(symbol.toLower());
    persistSettings();
}

void MainWindow::onOrderTypeChanged(int)
{
    updateOrderPriceVisibility();
}

void MainWindow::updateOrderPriceVisibility()
{
    const bool isLimit = currentOrderType() == OrderManager::OrderType::Limit;
    m_orderPriceEdit->setEnabled(isLimit);
    if (!isLimit) {
        m_orderPriceEdit->clear();
        m_orderPriceEdit->setPlaceholderText("Market (auto)");
    } else {
        m_orderPriceEdit->setPlaceholderText("Price");
    }
}

OrderManager::OrderType MainWindow::currentOrderType() const
{
    return (m_orderTypeCombo->currentIndex() == 1)
            ? OrderManager::OrderType::Limit
            : OrderManager::OrderType::Market;
}

void MainWindow::onPlaceOrder()
{
    if (!m_orderManager)
        return;

    const QString symbol = m_symbolEdit->text().trimmed().toUpper();
    const QString side = m_orderSideCombo->currentText().toUpper();
    const double quantity = m_orderQtyEdit->text().toDouble();
    double price = m_orderPriceEdit->text().toDouble();

    if (symbol.isEmpty() || quantity <= 0.0) {
        m_statusLabel->setText("⚠️ Invalid order");
        return;
    }

    const OrderManager::OrderType type = currentOrderType();
    if (type == OrderManager::OrderType::Limit && price <= 0.0) {
        m_statusLabel->setText("⚠️ Enter limit price");
        return;
    }

    if (type == OrderManager::OrderType::Market && price <= 0.0) {
        if (m_lastPrice <= 0.0) {
            m_statusLabel->setText("⚠️ Awaiting price data");
            return;
        }
        price = m_lastPrice;
    }

    const int id = m_orderManager->placeOrder(type, symbol, side, quantity, price);
    if (id < 0) {
        m_statusLabel->setText("⚠️ Order rejected");
    } else {
        m_statusLabel->setText("✅ Order sent");
        if (!m_watchlist.contains(symbol)) {
            m_watchlist.append(symbol);
            populateWatchlist(symbol);
            persistWatchlist();
        } else {
            populateWatchlist(symbol);
        }
    }
}

void MainWindow::onCancelSelectedOrder()
{
    if (!m_orderManager)
        return;
    auto selected = m_ordersTable->selectionModel()->selectedRows();
    if (selected.isEmpty())
        return;
    const int row = selected.first().row();
    bool ok = false;
    QTableWidgetItem *idItem = m_ordersTable->item(row, 0);
    if (!idItem)
        return;
    int id = idItem->data(Qt::UserRole).toInt(&ok);
    if (!ok)
        id = idItem->text().toInt(&ok);
    if (ok && m_orderManager->cancelOrder(id)) {
        m_statusLabel->setText("✅ Order cancelled");
    }
}

void MainWindow::onOrderSelectionChanged()
{
    const auto *selection = m_ordersTable->selectionModel();
    const bool hasSelection = selection && !selection->selectedRows().isEmpty();
    m_cancelOrderButton->setEnabled(hasSelection);
}

void MainWindow::refreshOrders(const QList<Order> &orders)
{
    m_ordersTable->setRowCount(orders.size());
    int row = 0;
    auto formatQuantity = [this](double value) {
        QString text = QString::number(value, 'f', m_quantityPrecision);
        if (text.contains('.')) {
            while (text.endsWith('0'))
                text.chop(1);
            if (text.endsWith('.'))
                text.chop(1);
        }
        return text;
    };
    QLocale locale;
    for (const Order &order : orders) {
        auto setItem = [&](int column, const QString &text) {
            auto *item = new QTableWidgetItem(text);
            item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
            m_ordersTable->setItem(row, column, item);
        };

        auto *idItem = new QTableWidgetItem(QString::number(order.id));
        idItem->setData(Qt::UserRole, order.id);
        idItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        m_ordersTable->setItem(row, 0, idItem);

        setItem(1, order.symbol);
        setItem(2, order.side);
        setItem(3, order.type);
        setItem(4, formatQuantity(order.quantity));
        const double displayPrice = order.filledPrice > 0.0 ? order.filledPrice : order.price;
        setItem(5, locale.toString(displayPrice, 'f', 2));
        setItem(6, order.status);
        ++row;
    }
    onOrderSelectionChanged();
}

void MainWindow::refreshPortfolio(const PortfolioSnapshot &snapshot, const QList<Position> &positions)
{
    QLocale locale;
    auto setCurrency = [&](QLabel *label, double value) {
        label->setText(locale.toCurrencyString(value));
    };
    auto applyPnLStyle = [](QLabel *label, double value) {
        if (value > 0.0) {
            label->setStyleSheet("color: #12d980;");
        } else if (value < 0.0) {
            label->setStyleSheet("color: #ff6476;");
        } else {
            label->setStyleSheet("");
        }
    };

    setCurrency(m_balanceLabel, snapshot.accountBalance);
    setCurrency(m_equityLabel, snapshot.equity);
    setCurrency(m_realizedLabel, snapshot.realizedPnL);
    setCurrency(m_unrealizedLabel, snapshot.unrealizedPnL);
    setCurrency(m_accountMarginLabel, snapshot.accountMargin);
    setCurrency(m_orderMarginLabel, snapshot.orderMargin);
    setCurrency(m_availableFundsLabel, snapshot.availableFunds);

    applyPnLStyle(m_realizedLabel, snapshot.realizedPnL);
    applyPnLStyle(m_unrealizedLabel, snapshot.unrealizedPnL);
    if (snapshot.availableFunds < 0.0) {
        m_availableFundsLabel->setStyleSheet("color: #ff6476;");
    } else {
        m_availableFundsLabel->setStyleSheet("");
    }

    m_positionsTable->setRowCount(positions.size());
    int row = 0;
    auto formatQuantity = [this](double value) {
        QString text = QString::number(value, 'f', m_quantityPrecision);
        if (text.contains('.')) {
            while (text.endsWith('0'))
                text.chop(1);
            if (text.endsWith('.'))
                text.chop(1);
        }
        return text;
    };
    for (const Position &pos : positions) {
        auto setItem = [&](int column, const QString &text) {
            auto *item = new QTableWidgetItem(text);
            item->setFlags(Qt::ItemIsEnabled);
            m_positionsTable->setItem(row, column, item);
        };
        setItem(0, pos.symbol);
        setItem(1, formatQuantity(pos.qty));
        setItem(2, locale.toString(pos.avgPx, 'f', 2));
        setItem(3, locale.toString(pos.lastPrice, 'f', 2));
        setItem(4, locale.toString(pos.unrealizedPnL, 'f', 2));
        ++row;
    }
}

void MainWindow::loadStateFromStorage()
{
    QString symbolPreference;
    int feedIndex = m_feedSelector->currentIndex();

    if (m_storage) {
        m_watchlist = m_storage->loadWatchlist();
        if (m_watchlist.isEmpty()) {
            m_watchlist = {"BTCUSDT", "ETHUSDT", "EURUSD"};
        }

        const QJsonObject settings = m_storage->loadSettings();
        symbolPreference = settings.value("lastSymbol").toString();
        feedIndex = settings.value("feedMode").toInt(feedIndex);
    } else {
        m_watchlist = {"BTCUSDT", "ETHUSDT", "EURUSD"};
    }

    if (symbolPreference.isEmpty()) {
        symbolPreference = QStringLiteral("btcusdt");
    }

    m_symbolEdit->setText(symbolPreference);
    m_feedSelector->setCurrentIndex(feedIndex);
    onFeedModeChanged(feedIndex);

    populateWatchlist(symbolPreference.toUpper());
}

void MainWindow::populateWatchlist(const QString &selectSymbol)
{
    const QString desired = selectSymbol.isEmpty()
            ? (m_watchlistView->currentItem() ? m_watchlistView->currentItem()->text() : QString())
            : selectSymbol;

    m_watchlistView->clear();
    int indexToSelect = -1;
    for (int i = 0; i < m_watchlist.size(); ++i) {
        const QString &sym = m_watchlist.at(i);
        m_watchlistView->addItem(sym);
        if (!desired.isEmpty() && sym.compare(desired, Qt::CaseInsensitive) == 0) {
            indexToSelect = i;
        }
    }

    if (indexToSelect >= 0) {
        m_watchlistView->setCurrentRow(indexToSelect);
    } else if (!m_watchlist.isEmpty()) {
        m_watchlistView->setCurrentRow(0);
    }
    m_watchlistRemoveButton->setEnabled(!m_watchlist.isEmpty());
}

void MainWindow::persistWatchlist()
{
    if (m_storage)
        m_storage->saveWatchlist(m_watchlist);
}

void MainWindow::persistSettings()
{
    if (!m_storage)
        return;
    QJsonObject settings;
    settings.insert("lastSymbol", m_symbolEdit->text().trimmed());
    settings.insert("feedMode", m_feedSelector->currentIndex());
    m_storage->saveSettings(settings);
}
