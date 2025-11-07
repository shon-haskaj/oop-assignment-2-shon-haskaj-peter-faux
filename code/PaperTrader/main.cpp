#include "core/papertraderapp.h"
#include "ui/controllers/chartcontroller.h"
#include "ui/controllers/tradingcontroller.h"
#include "ui/mainwindow.h"
#include <QApplication>
#include <QObject>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    PaperTraderApp coreApp;
    ChartController chartController(coreApp.chartManager());
    TradingController tradingController(&coreApp);
    QObject::connect(&chartController, &ChartController::lastPriceChanged,
                     &tradingController, &TradingController::onLastPriceChanged);

    MainWindow w(&chartController, &tradingController);
    w.showMaximized();
    return app.exec();
}
