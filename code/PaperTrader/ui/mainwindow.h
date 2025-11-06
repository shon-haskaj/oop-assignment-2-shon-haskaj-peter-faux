#pragma once
#include <QMainWindow>
#include <QComboBox>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include "core/papertraderapp.h"
#include "chartwidget.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onStartFeed();
    void onStopFeed();
    void onFeedModeChanged(int index);

private:
    PaperTraderApp *m_app;
    ChartWidget    *m_chart;

    // Toolbar widgets
    QComboBox  *m_feedSelector;
    QLineEdit  *m_symbolEdit;
    QPushButton *m_startButton;
    QPushButton *m_stopButton;
    QLabel     *m_statusLabel;

    MarketDataProvider::FeedMode m_currentMode = MarketDataProvider::FeedMode::Synthetic;

    void setupUi();
    void setupConnections();
};
