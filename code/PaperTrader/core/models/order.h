#pragma once
#include <QString>
#include <QDateTime>

struct Order {
    int id = 0;
    QString symbol;
    double price = 0.0;
    int quantity = 0;
    QString side;     // "BUY" / "SELL"
    QString type;     // "Market", "Limit", etc.
    QString status;   // "Open", "Filled", "Cancelled"
    QDateTime timestamp;
};
