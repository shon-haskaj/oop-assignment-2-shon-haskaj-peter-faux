#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QStatusBar>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
    m_app(new PaperTraderApp(this)),
    m_chart(new ChartWidget(this))
{
    setupUi();
    setupConnections();

    m_symbolEdit->setText("btcusdt");
    m_statusLabel->setText("🔴 Disconnected");
    m_feedSelector->setCurrentIndex(0);
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUi()
{
    QWidget *central = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(central);

    // --- Control Panel ---
    QFrame *toolbar = new QFrame(this);
    toolbar->setFrameShape(QFrame::NoFrame);
    QHBoxLayout *toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(0, 0, 0, 0);
    toolbarLayout->setSpacing(8);

    m_feedSelector = new QComboBox(this);
    m_feedSelector->addItems({"Synthetic", "Binance", "Finnhub"});

    m_symbolEdit = new QLineEdit(this);
    m_symbolEdit->setPlaceholderText("Symbol (e.g. btcusdt or eurusd)");

    m_startButton = new QPushButton("Start Feed", this);
    m_stopButton  = new QPushButton("Stop Feed", this);
    m_statusLabel = new QLabel("🔴 Disconnected", this);

    toolbarLayout->addWidget(new QLabel("Feed:", this));
    toolbarLayout->addWidget(m_feedSelector);
    toolbarLayout->addWidget(new QLabel("Symbol:", this));
    toolbarLayout->addWidget(m_symbolEdit, 1);
    toolbarLayout->addWidget(m_startButton);
    toolbarLayout->addWidget(m_stopButton);
    toolbarLayout->addWidget(m_statusLabel);

    // --- Chart area ---
    mainLayout->addWidget(toolbar);
    mainLayout->addWidget(m_chart, 1);

    setCentralWidget(central);
    setWindowTitle("PaperTrader - Market Feed Viewer");
    resize(1000, 600);
}

void MainWindow::setupConnections()
{
    connect(m_feedSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onFeedModeChanged);

    connect(m_startButton, &QPushButton::clicked,
            this, &MainWindow::onStartFeed);
    connect(m_stopButton, &QPushButton::clicked,
            this, &MainWindow::onStopFeed);

    connect(m_app->findChild<MarketDataProvider*>(),
            &MarketDataProvider::newCandle,
            m_chart,
            &ChartWidget::appendCandle);

    connect(m_app->findChild<MarketDataProvider*>(),
            &MarketDataProvider::connectionStateChanged,
            this, [this](bool connected) {
                m_statusLabel->setText(connected ? "🟢 Connected" : "🔴 Disconnected");
            });
}

void MainWindow::onFeedModeChanged(int index)
{
    switch (index) {
    case 0: m_currentMode = MarketDataProvider::FeedMode::Synthetic; break;
    case 1: m_currentMode = MarketDataProvider::FeedMode::Binance; break;
    case 2: m_currentMode = MarketDataProvider::FeedMode::Finnhub; break;
    default: m_currentMode = MarketDataProvider::FeedMode::Synthetic; break;
    }
}

void MainWindow::onStartFeed()
{
    QString symbol = m_symbolEdit->text().trimmed();
    if (symbol.isEmpty())
        symbol = "btcusdt";

    auto provider = m_app->findChild<MarketDataProvider*>();
    if (!provider) {
        qWarning() << "No MarketDataProvider found!";
        return;
    }

    provider->stopFeed();
    provider->startFeed(m_currentMode, symbol);

    setWindowTitle(QString("PaperTrader - %1 (%2)")
                       .arg(symbol.toUpper())
                       .arg(m_feedSelector->currentText()));
}

void MainWindow::onStopFeed()
{
    auto provider = m_app->findChild<MarketDataProvider*>();
    if (provider) provider->stopFeed();

    m_chart->clearCandles();  // ✅ reset chart
    m_statusLabel->setText("🔴 Disconnected");
    setWindowTitle("PaperTrader - Market Feed Viewer");
}
