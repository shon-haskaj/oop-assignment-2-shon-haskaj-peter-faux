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
#include <QVariantAnimation>
#include <QEasingCurve>
#include <QAbstractAnimation>
#include <QSignalBlocker>
#include <QStyle>

#include <algorithm>
#include <functional>

namespace {
void animateSplitterSizes(QSplitter *splitter,
                          const QList<int> &startSizes,
                          const QList<int> &targetSizes,
                          const std::function<void()> &finished = {})
{
    if (!splitter) {
        if (finished)
            finished();
        return;
    }

    if (startSizes.size() != targetSizes.size()) {
        splitter->setSizes(targetSizes);
        if (finished)
            finished();
        return;
    }

    if (startSizes == targetSizes) {
        splitter->setSizes(targetSizes);
        if (finished)
            finished();
        return;
    }

    auto *animation = new QVariantAnimation(splitter);
    animation->setDuration(200);
    animation->setEasingCurve(QEasingCurve::InOutCubic);
    animation->setStartValue(0.0);
    animation->setEndValue(1.0);
    QObject::connect(animation, &QVariantAnimation::valueChanged,
                     splitter, [splitter, startSizes, targetSizes](const QVariant &value) {
                         const double progress = value.toDouble();
                         QList<int> newSizes;
                         newSizes.reserve(targetSizes.size());
                         for (int i = 0; i < targetSizes.size(); ++i) {
                             const int start = startSizes.at(i);
                             const int end = targetSizes.at(i);
                             const double interpolated = start + (end - start) * progress;
                             newSizes.append(static_cast<int>(interpolated));
                         }
                         splitter->setSizes(newSizes);
                     });
    QObject::connect(animation, &QVariantAnimation::finished,
                     splitter, [finished]() {
                         if (finished)
                             finished();
                     });
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}
}

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

    m_themeToggle = new QToolButton(this);
    m_themeToggle->setObjectName("themeToggle");
    m_themeToggle->setCheckable(true);
    m_themeToggle->setChecked(false);
    m_themeToggle->setText(QStringLiteral("🌙"));
    m_themeToggle->setToolTip(tr("Switch to Light Mode"));
    m_themeToggle->setAutoRaise(false);
    toolbarLayout->addWidget(m_themeToggle);

    m_watchlistToggle = new QToolButton(this);
    m_watchlistToggle->setObjectName("panelToggle");
    m_watchlistToggle->setCheckable(true);
    m_watchlistToggle->setChecked(true);
    m_watchlistToggle->setIcon(style()->standardIcon(QStyle::SP_FileDialogListView));
    m_watchlistToggle->setToolTip(tr("Hide watchlist"));
    m_watchlistToggle->setAutoRaise(false);
    m_watchlistToggle->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_watchlistToggle->setProperty("panelRole", "watchlist");
    m_watchlistToggle->setToolButtonStyle(Qt::ToolButtonIconOnly);

    m_orderToggleButton = new QToolButton(this);
    m_orderToggleButton->setObjectName("panelToggle");
    m_orderToggleButton->setCheckable(true);
    m_orderToggleButton->setChecked(true);
    m_orderToggleButton->setText(tr("Trade"));
    m_orderToggleButton->setToolTip(tr("Hide trade ticket"));
    m_orderToggleButton->setAutoRaise(false);
    m_orderToggleButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_orderToggleButton->setProperty("panelRole", "trade");
    m_orderToggleButton->setToolButtonStyle(Qt::ToolButtonTextOnly);

    m_portfolioToggleButton = new QToolButton(this);
    m_portfolioToggleButton->setObjectName("panelToggle");
    m_portfolioToggleButton->setCheckable(true);
    m_portfolioToggleButton->setChecked(true);
    m_portfolioToggleButton->setText(tr("Portfolio"));
    m_portfolioToggleButton->setToolTip(tr("Hide portfolio"));
    m_portfolioToggleButton->setAutoRaise(false);
    m_portfolioToggleButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_portfolioToggleButton->setProperty("panelRole", "portfolio");
    m_portfolioToggleButton->setToolButtonStyle(Qt::ToolButtonTextOnly);

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
    horizontalSplit->setCollapsible(1, true);
    horizontalSplit->setCollapsible(2, true);

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
    verticalSplit->setCollapsible(1, true);

    m_horizontalSplit = horizontalSplit;
    m_verticalSplit = verticalSplit;
    m_chartPanelWidget = chartPanel;
    m_orderPanelWidget = orderPanel;
    m_watchlistPanelWidget = watchlistPanel;
    m_topAreaWidget = topArea;
    m_portfolioPanelWidget = portfolioPanel;

    m_savedWatchlistWidth = watchlistPanel->sizeHint().width();
    m_savedOrderWidth = orderPanel->sizeHint().width();
    m_savedPortfolioHeight = portfolioPanel->sizeHint().height();

    QWidget *edgeDock = new QWidget(this);
    edgeDock->setObjectName("edgeDock");
    edgeDock->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    QVBoxLayout *edgeLayout = new QVBoxLayout(edgeDock);
    edgeLayout->setContentsMargins(0, 0, 0, 0);
    edgeLayout->setSpacing(12);
    edgeLayout->addWidget(m_watchlistToggle, 0, Qt::AlignRight | Qt::AlignTop);
    edgeLayout->addWidget(m_orderToggleButton, 0, Qt::AlignRight | Qt::AlignTop);
    edgeLayout->addStretch(1);
    edgeLayout->addWidget(m_portfolioToggleButton, 0, Qt::AlignRight | Qt::AlignBottom);

    QHBoxLayout *contentRow = new QHBoxLayout();
    contentRow->setContentsMargins(0, 0, 0, 0);
    contentRow->setSpacing(10);
    contentRow->addWidget(verticalSplit, 1);
    contentRow->addWidget(edgeDock, 0);

    mainLayout->addWidget(toolbar);
    mainLayout->addLayout(contentRow, 1);

    toggleOrderPanel(true, false);
    toggleWatchlistPanel(true, false);
    togglePortfolioPanel(true, false);

    setCentralWidget(central);
    setWindowTitle("PaperTrader - Market Feed Viewer");
    resize(1280, 720);
    statusBar()->setVisible(false);

    applyTheme(m_theme);
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
    m_watchlistAddButton->setProperty("accent", "neutral");
    m_watchlistRemoveButton->setProperty("accent", "neutral");
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
    m_placeOrderButton->setObjectName("tradeActionButton");
    m_placeOrderButton->setProperty("accent", "buy");
    m_cancelOrderButton->setProperty("accent", "neutral");
    buttonRow->addWidget(m_placeOrderButton, 1);
    buttonRow->addWidget(m_cancelOrderButton, 1);
    containerLayout->addLayout(buttonRow);

    QLabel *ordersHeader = new QLabel("Orders", panel);
    ordersHeader->setObjectName("sectionTitle");
    containerLayout->addWidget(ordersHeader);

    m_ordersTable = new QTableWidget(panel);
    m_ordersTable->setColumnCount(10);
    m_ordersTable->setHorizontalHeaderLabels({"ID", "Symbol", "Side", "Type", "Requested", "Working", "Filled", "Price", "Fee", "Status"});
    m_ordersTable->horizontalHeader()->setStretchLastSection(true);
    m_ordersTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_ordersTable->verticalHeader()->setVisible(false);
    m_ordersTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_ordersTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_ordersTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    containerLayout->addWidget(m_ordersTable, 1);

    layout->addWidget(m_orderContainer);

    updateOrderPriceVisibility();
    updateOrderButtonAccent();
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
    connect(m_orderSideCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onOrderSideChanged);
    connect(m_placeOrderButton, &QPushButton::clicked,
            this, &MainWindow::onPlaceOrder);
    connect(m_cancelOrderButton, &QPushButton::clicked,
            this, &MainWindow::onCancelSelectedOrder);
    connect(m_ordersTable, &QTableWidget::itemSelectionChanged,
            this, &MainWindow::onOrderSelectionChanged);

    connect(m_themeToggle, &QToolButton::toggled,
            this, &MainWindow::onThemeToggled);

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
        connect(m_orderManager, &OrderManager::orderRejected,
                this, &MainWindow::onOrderRejected);
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
    toggleWatchlistPanel(expanded, true);
}

void MainWindow::onOrderPanelToggled(bool expanded)
{
    toggleOrderPanel(expanded, true);
}

void MainWindow::onPortfolioToggled(bool expanded)
{
    togglePortfolioPanel(expanded, true);
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

void MainWindow::onOrderSideChanged(int)
{
    updateOrderButtonAccent();
}

void MainWindow::updateOrderButtonAccent()
{
    if (!m_placeOrderButton || !m_orderSideCombo)
        return;

    const QString side = m_orderSideCombo->currentText();
    const bool isBuy = side.compare(QStringLiteral("Buy"), Qt::CaseInsensitive) == 0;
    m_placeOrderButton->setProperty("accent", isBuy ? "buy" : "sell");
    m_placeOrderButton->setText(isBuy ? tr("Buy") : tr("Sell / Short"));
    m_placeOrderButton->style()->unpolish(m_placeOrderButton);
    m_placeOrderButton->style()->polish(m_placeOrderButton);
    m_placeOrderButton->update();
}

void MainWindow::updateToggleButtonState(QToolButton *button, bool active)
{
    if (!button)
        return;
    button->setProperty("active", active);
    button->setChecked(active);
    button->style()->unpolish(button);
    button->style()->polish(button);
    button->update();
}

QString MainWindow::errorCodeToMessage(const QString &code) const
{
    const QString upper = code.toUpper();
    if (upper == QLatin1String("ERR_INVALID_QTY"))
        return tr("Quantity must be positive");
    if (upper == QLatin1String("ERR_INVALID_PRICE"))
        return tr("Enter a valid price");
    if (upper == QLatin1String("ERR_INVALID_SYMBOL"))
        return tr("Enter a symbol");
    if (upper == QLatin1String("ERR_INVALID_SIDE"))
        return tr("Unsupported order side");
    if (upper == QLatin1String("ERR_INSUFFICIENT_FUNDS"))
        return tr("Insufficient available funds");
    if (upper == QLatin1String("ERR_INSUFFICIENT_MARGIN"))
        return tr("Insufficient margin");
    if (upper == QLatin1String("ERR_PARTIAL_FILL"))
        return tr("Partial fill");
    return code;
}

void MainWindow::applyTheme(Theme theme)
{
    m_theme = theme;
    if (m_themeToggle) {
        const QSignalBlocker blocker(m_themeToggle);
        const bool light = theme == Theme::Light;
        m_themeToggle->setChecked(light);
        m_themeToggle->setText(light ? QStringLiteral("☀️") : QStringLiteral("🌙"));
        m_themeToggle->setToolTip(light ? tr("Switch to Dark Mode")
                                        : tr("Switch to Light Mode"));
        m_themeToggle->style()->unpolish(m_themeToggle);
        m_themeToggle->style()->polish(m_themeToggle);
        m_themeToggle->update();
    }

    setStyleSheet(styleSheetForTheme(theme));
    updateOrderButtonAccent();
    updateToggleButtonState(m_watchlistToggle, m_watchlistToggle && m_watchlistToggle->isChecked());
    updateToggleButtonState(m_orderToggleButton, m_orderToggleButton && m_orderToggleButton->isChecked());
    updateToggleButtonState(m_portfolioToggleButton, m_portfolioToggleButton && m_portfolioToggleButton->isChecked());
}

QString MainWindow::styleSheetForTheme(Theme theme) const
{
    if (theme == Theme::Light) {
        return QStringLiteral(
            "QWidget { color: #1b2332; background-color: #f5f6fa; font-family: 'Inter', 'Segoe UI', sans-serif; }"
            "QFrame#toolbar { background-color: #ffffff; border-radius: 10px; border: 1px solid #dce1f0; }"
            "QFrame#watchlistPanel, QFrame#orderPanel, QFrame#portfolioPanel { background-color: #ffffff; border-radius: 10px; border: 1px solid #d4d9e5; }"
            "QLineEdit, QComboBox { background-color: #eef1f8; border: 1px solid #c7cedd; border-radius: 6px; padding: 4px 8px; color: #1b2332; }"
            "QPushButton#tradeActionButton[accent=\"buy\"] { background-color: #2f6df7; color: #ffffff; border-radius: 6px; padding: 6px 16px; font-weight: 600; }"
            "QPushButton#tradeActionButton[accent=\"buy\"]:hover { background-color: #3a77ff; }"
            "QPushButton#tradeActionButton[accent=\"sell\"] { background-color: #e04646; color: #ffffff; border-radius: 6px; padding: 6px 16px; font-weight: 600; }"
            "QPushButton#tradeActionButton[accent=\"sell\"]:hover { background-color: #eb5959; }"
            "QPushButton[accent=\"neutral\"] { background-color: #d7dcea; color: #1b2332; border-radius: 6px; padding: 6px 14px; }"
            "QPushButton[accent=\"neutral\"]:hover { background-color: #c9cfe0; }"
            "QToolButton#panelToggle { min-width: 44px; min-height: 44px; border-radius: 14px; border: 1px solid #ccd2e2; background-color: #eef1f8; color: #4a556f; font-weight: 600; }"
            "QToolButton#panelToggle[active=\"true\"] { background-color: #d9def0; color: #1b2332; }"
            "QToolButton#panelToggle:hover { background-color: #d0d6ea; }"
            "QToolButton#themeToggle { border: 1px solid #ccd2e2; border-radius: 12px; background-color: #eef1f8; padding: 4px 10px; }"
            "QToolButton#themeToggle:hover { background-color: #d9def0; }"
            "QListWidget, QTableWidget { background-color: #ffffff; border: 1px solid #d4d9e5; border-radius: 8px; color: #1b2332; }"
            "QHeaderView::section { background-color: #eef1f8; border: none; color: #4a556f; }"
            "QLabel#sectionTitle { font-weight: 600; font-size: 14px; color: #4a556f; }"
            "QLabel#metricValue { font-size: 18px; font-weight: 600; color: #1b2332; }"
        );
    }

    return QStringLiteral(
        "QWidget { color: #e6ecf4; background-color: #10131b; font-family: 'Inter', 'Segoe UI', sans-serif; }"
        "QFrame#toolbar { background-color: #191e2b; border-radius: 10px; }"
        "QFrame#watchlistPanel, QFrame#orderPanel, QFrame#portfolioPanel { background-color: #161b2a; border-radius: 10px; border: 1px solid rgba(70, 80, 110, 0.6); }"
        "QLineEdit, QComboBox { background-color: #0f1320; border: 1px solid #2f3850; border-radius: 6px; padding: 4px 8px; color: #e6ecf4; }"
        "QPushButton#tradeActionButton[accent=\"buy\"] { background-color: #3366ff; color: #ffffff; border-radius: 6px; padding: 6px 16px; font-weight: 600; }"
        "QPushButton#tradeActionButton[accent=\"buy\"]:hover { background-color: #3f74ff; }"
        "QPushButton#tradeActionButton[accent=\"sell\"] { background-color: #ef5361; color: #ffffff; border-radius: 6px; padding: 6px 16px; font-weight: 600; }"
        "QPushButton#tradeActionButton[accent=\"sell\"]:hover { background-color: #f16d78; }"
        "QPushButton[accent=\"neutral\"] { background-color: #2a3145; color: #d7dbea; border-radius: 6px; padding: 6px 14px; }"
        "QPushButton[accent=\"neutral\"]:hover { background-color: #343c56; }"
        "QToolButton#panelToggle { min-width: 44px; min-height: 44px; border-radius: 14px; border: 1px solid rgba(70, 80, 110, 0.7); background-color: #121622; color: #aeb6d9; font-weight: 600; }"
        "QToolButton#panelToggle[active=\"true\"] { background-color: #1f2640; color: #eef2ff; }"
        "QToolButton#panelToggle:hover { background-color: #283051; }"
        "QToolButton#themeToggle { border: 1px solid rgba(70, 80, 110, 0.7); border-radius: 12px; background-color: #121622; padding: 4px 10px; color: #e6ecf4; }"
        "QToolButton#themeToggle:hover { background-color: #1f2640; }"
        "QListWidget, QTableWidget { background-color: #0f1320; border: 1px solid #2f3850; border-radius: 8px; color: #e6ecf4; }"
        "QHeaderView::section { background-color: #1b2133; border: none; color: #aeb6d9; }"
        "QLabel#sectionTitle { font-weight: 600; font-size: 14px; color: #aeb6d9; }"
        "QLabel#metricValue { font-size: 18px; font-weight: 600; color: #e6ecf4; }"
    );
}

void MainWindow::onOrderRejected(const QString &symbol, const QString &errorCode, double rejectedQuantity)
{
    QString message = errorCodeToMessage(errorCode);
    if (rejectedQuantity > 0.0) {
        QLocale locale;
        message = tr("%1 (rejected %2)")
                      .arg(message,
                           locale.toString(rejectedQuantity, 'f', m_quantityPrecision));
    }
    m_statusLabel->setText(tr("⚠️ %1 [%2]").arg(message, symbol));
}

void MainWindow::onThemeToggled(bool checked)
{
    applyTheme(checked ? Theme::Light : Theme::Dark);
}

void MainWindow::toggleWatchlistPanel(bool expanded, bool animate)
{
    if (m_watchlistToggle) {
        if (m_watchlistToggle->isChecked() != expanded) {
            const QSignalBlocker blocker(m_watchlistToggle);
            m_watchlistToggle->setChecked(expanded);
        }
        m_watchlistToggle->setToolTip(expanded ? tr("Hide watchlist")
                                               : tr("Show watchlist"));
        updateToggleButtonState(m_watchlistToggle, expanded);
    }

    if (!m_horizontalSplit || !m_watchlistPanelWidget || !m_chartPanelWidget)
        return;

    QList<int> currentSizes = m_horizontalSplit->sizes();
    const int watchIndex = m_horizontalSplit->indexOf(m_watchlistPanelWidget);
    const int chartIndex = m_horizontalSplit->indexOf(m_chartPanelWidget);

    if (watchIndex < 0 || chartIndex < 0)
        return;

    if (!expanded) {
        const int currentWidth = currentSizes.value(watchIndex);
        if (currentWidth > 0)
            m_savedWatchlistWidth = currentWidth;
    } else if (m_savedWatchlistWidth <= 0) {
        m_savedWatchlistWidth = std::max(m_watchlistPanelWidget->sizeHint().width(), 220);
    }

    const int desiredWidth = expanded ? m_savedWatchlistWidth : 0;
    QList<int> targetSizes = currentSizes;
    const int delta = currentSizes.value(watchIndex) - desiredWidth;
    targetSizes[watchIndex] = desiredWidth;
    targetSizes[chartIndex] = std::max(1, currentSizes.value(chartIndex) + delta);

    auto finish = [this, expanded]() {
        if (m_watchlistPanelWidget)
            m_watchlistPanelWidget->setVisible(expanded);
        if (m_watchlistContainer)
            m_watchlistContainer->setVisible(expanded);
    };

    if (expanded) {
        if (m_watchlistPanelWidget)
            m_watchlistPanelWidget->setVisible(true);
        if (m_watchlistContainer)
            m_watchlistContainer->setVisible(true);
    }

    if (!animate) {
        m_horizontalSplit->setSizes(targetSizes);
        finish();
    } else {
        animateSplitterSizes(m_horizontalSplit, currentSizes, targetSizes, finish);
    }
}

void MainWindow::toggleOrderPanel(bool expanded, bool animate)
{
    if (m_orderToggleButton) {
        if (m_orderToggleButton->isChecked() != expanded) {
            const QSignalBlocker blocker(m_orderToggleButton);
            m_orderToggleButton->setChecked(expanded);
        }
        m_orderToggleButton->setToolTip(expanded ? tr("Hide trade ticket")
                                                 : tr("Show trade ticket"));
        updateToggleButtonState(m_orderToggleButton, expanded);
    }

    if (!m_horizontalSplit || !m_orderPanelWidget || !m_chartPanelWidget)
        return;

    QList<int> currentSizes = m_horizontalSplit->sizes();
    const int orderIndex = m_horizontalSplit->indexOf(m_orderPanelWidget);
    const int chartIndex = m_horizontalSplit->indexOf(m_chartPanelWidget);

    if (orderIndex < 0 || chartIndex < 0)
        return;

    if (!expanded) {
        const int currentWidth = currentSizes.value(orderIndex);
        if (currentWidth > 0)
            m_savedOrderWidth = currentWidth;
    } else if (m_savedOrderWidth <= 0) {
        m_savedOrderWidth = std::max(m_orderPanelWidget->sizeHint().width(), 220);
    }

    const int desiredWidth = expanded ? m_savedOrderWidth : 0;
    QList<int> targetSizes = currentSizes;
    const int delta = currentSizes.value(orderIndex) - desiredWidth;
    targetSizes[orderIndex] = desiredWidth;
    targetSizes[chartIndex] = std::max(1, currentSizes.value(chartIndex) + delta);

    auto finish = [this, expanded]() {
        if (m_orderPanelWidget)
            m_orderPanelWidget->setVisible(expanded);
        if (m_orderContainer)
            m_orderContainer->setVisible(expanded);
    };

    if (expanded) {
        if (m_orderPanelWidget)
            m_orderPanelWidget->setVisible(true);
        if (m_orderContainer)
            m_orderContainer->setVisible(true);
    }

    if (!animate) {
        m_horizontalSplit->setSizes(targetSizes);
        finish();
    } else {
        animateSplitterSizes(m_horizontalSplit, currentSizes, targetSizes, finish);
    }
}

void MainWindow::togglePortfolioPanel(bool expanded, bool animate)
{
    if (m_portfolioToggleButton) {
        if (m_portfolioToggleButton->isChecked() != expanded) {
            const QSignalBlocker blocker(m_portfolioToggleButton);
            m_portfolioToggleButton->setChecked(expanded);
        }
        m_portfolioToggleButton->setToolTip(expanded ? tr("Hide portfolio")
                                                     : tr("Show portfolio"));
        updateToggleButtonState(m_portfolioToggleButton, expanded);
    }

    if (m_portfolioContent)
        m_portfolioContent->setVisible(expanded);

    if (!m_verticalSplit || !m_portfolioPanelWidget || !m_topAreaWidget)
        return;

    QList<int> currentSizes = m_verticalSplit->sizes();
    const int portfolioIndex = m_verticalSplit->indexOf(m_portfolioPanelWidget);
    const int topIndex = m_verticalSplit->indexOf(m_topAreaWidget);

    if (portfolioIndex < 0 || topIndex < 0)
        return;

    if (!expanded) {
        const int currentHeight = currentSizes.value(portfolioIndex);
        if (currentHeight > 0)
            m_savedPortfolioHeight = currentHeight;
    } else if (m_savedPortfolioHeight <= 0) {
        m_savedPortfolioHeight = std::max(m_portfolioPanelWidget->sizeHint().height(), 200);
    }

    const int desiredHeight = expanded ? m_savedPortfolioHeight : 0;
    QList<int> targetSizes = currentSizes;
    const int delta = currentSizes.value(portfolioIndex) - desiredHeight;
    targetSizes[portfolioIndex] = desiredHeight;
    targetSizes[topIndex] = std::max(1, currentSizes.value(topIndex) + delta);

    auto finish = [this, expanded]() {
        if (m_portfolioPanelWidget)
            m_portfolioPanelWidget->setVisible(expanded);
        if (m_portfolioContent)
            m_portfolioContent->setVisible(expanded);
    };

    if (expanded && m_portfolioPanelWidget)
        m_portfolioPanelWidget->setVisible(true);

    if (!animate) {
        m_verticalSplit->setSizes(targetSizes);
        finish();
    } else {
        animateSplitterSizes(m_verticalSplit, currentSizes, targetSizes, finish);
    }
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

    const OrderManager::OrderPlacementResult result = m_orderManager->placeOrder(type, symbol, side, quantity, price);

    if (!result.accepted) {
        m_statusLabel->setText(QStringLiteral("❌ %1").arg(errorCodeToMessage(result.errorCode)));
        return;
    }

    QString statusText;
    if (result.partial) {
        const QString reason = result.errorCode.isEmpty()
                ? tr("Partial fill")
                : errorCodeToMessage(result.errorCode);
        const double rejectedQty = result.rejectedQuantity;
        if (rejectedQty > 0.0) {
            auto formatQty = [this](double value) {
                QString text = QString::number(value, 'f', m_quantityPrecision);
                if (text.contains('.')) {
                    while (text.endsWith('0'))
                        text.chop(1);
                    if (text.endsWith('.'))
                        text.chop(1);
                }
                return text;
            };
            statusText = tr("⚠️ %1 (rejected %2)")
                    .arg(reason, formatQty(rejectedQty));
        } else {
            statusText = tr("⚠️ %1").arg(reason);
        }
    } else {
        statusText = tr("✅ Order sent");
    }
    m_statusLabel->setText(statusText);

    if (!m_watchlist.contains(symbol)) {
        m_watchlist.append(symbol);
        populateWatchlist(symbol);
        persistWatchlist();
    } else {
        populateWatchlist(symbol);
    }
}

void MainWindow::onCancelSelectedOrder()
{
    if (!m_orderManager)
        return;
    auto *selection = m_ordersTable->selectionModel();
    if (!selection)
        return;
    const QModelIndexList selected = selection->selectedRows();
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
    const bool hasSelection = !m_ordersTable->selectedItems().isEmpty();
    if (m_cancelOrderButton)
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
        auto makeItem = [&](int column, const QString &text) {
            auto *item = new QTableWidgetItem(text);
            item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
            if (column == 9 && !order.errorCode.isEmpty())
                item->setToolTip(errorCodeToMessage(order.errorCode));
            m_ordersTable->setItem(row, column, item);
        };

        auto *idItem = new QTableWidgetItem(QString::number(order.id));
        idItem->setData(Qt::UserRole, order.id);
        idItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        m_ordersTable->setItem(row, 0, idItem);

        makeItem(1, order.symbol);
        makeItem(2, order.side);
        makeItem(3, order.type);
        const double requested = order.requestedQuantity > 0.0
                ? order.requestedQuantity
                : order.quantity;
        makeItem(4, formatQuantity(requested));
        makeItem(5, formatQuantity(order.quantity));
        makeItem(6, formatQuantity(order.filledQuantity));
        const double displayPrice = order.filledPrice > 0.0 ? order.filledPrice : order.price;
        makeItem(7, locale.toString(displayPrice, 'f', 2));
        makeItem(8, locale.toString(order.fee, 'f', 2));
        QString status = order.status;
        if (!order.errorCode.isEmpty())
            status.append(QStringLiteral(" (%1)").arg(order.errorCode));
        makeItem(9, status);
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
