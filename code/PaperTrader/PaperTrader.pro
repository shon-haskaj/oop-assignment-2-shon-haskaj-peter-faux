QT += core gui widgets network websockets
CONFIG += c++20
TEMPLATE = app
TARGET = PaperTrader
# --- Local-only key for testing (REMOVE before commit) ---
DEFINES += FINNHUB_KEY=\\\"YOUR_FINNHUB_KEY_HERE\\\"



# -------------------------------------------------------
# --- Source files ---
SOURCES += \
    main.cpp \
    core/papertraderapp.cpp \
    core/marketdataprovider.cpp \
    core/chartmanager.cpp \
    core/ordermanager.cpp \
    core/portfoliomanager.cpp \
    core/storagemanager.cpp \
    ui/mainwindow.cpp \
    ui/chartwidget.cpp \
    ui/controllers/tradingcontroller.cpp \
    ui/controllers/chartcontroller.cpp

# --- Header files ---
HEADERS += \
    core/papertraderapp.h \
    core/marketdataprovider.h \
    core/chartmanager.h \
    core/ordermanager.h \
    core/portfoliomanager.h \
    core/storagemanager.h \
    core/models/candle.h \
    core/models/quote.h \
    core/models/order.h \
    core/models/executionreport.h \
    core/models/position.h \
    ui/mainwindow.h \
    ui/chartwidget.h \
    ui/controllers/tradingcontroller.h \
    ui/controllers/chartcontroller.h

# --- UI / Resources (enable once created) ---
# FORMS += ui/mainwindow.ui
# RESOURCES += resources/icons.qrc

# --- Include paths ---
INCLUDEPATH += \
    core \
    core/models \
    ui \
    ui/controllers

# --- Build output directories ---
DESTDIR     = $$PWD/bin
OBJECTS_DIR = $$PWD/build/obj
MOC_DIR     = $$PWD/build/moc
RCC_DIR     = $$PWD/build/rcc
UI_DIR      = $$PWD/build/ui
