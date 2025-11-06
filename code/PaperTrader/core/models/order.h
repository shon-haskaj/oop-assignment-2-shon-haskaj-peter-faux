#pragma once
#include <QString>
#include <QDateTime>

struct Order {
    int id = 0;
    QString symbol;
    double price = 0.0;
    double quantity = 0.0;
    double requestedQuantity = 0.0;
    QString side;     // "BUY" / "SELL"
    QString type;     // "Market", "Limit", etc.
    QString status;   // "Open", "Filled", "Cancelled"
    QDateTime timestamp;
    double filledPrice = 0.0;
    double filledQuantity = 0.0;
    double fee = 0.0;
    QString errorCode;
};
