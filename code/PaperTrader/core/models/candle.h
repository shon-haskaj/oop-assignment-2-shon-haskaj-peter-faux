#pragma once
#include <QDateTime>
#include <QString>

struct Candle {
    QDateTime timestamp;
    double open = 0.0;
    double high = 0.0;
    double low  = 0.0;
    double close = 0.0;
    double volume = 0.0;
    QString symbol;   // ✅ added field
};
