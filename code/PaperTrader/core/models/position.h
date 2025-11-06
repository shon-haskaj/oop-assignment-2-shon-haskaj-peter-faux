#ifndef POSITION_H
#define POSITION_H
#include <QString>

struct Position {
    QString symbol;
    double qty{};
    double avgPx{};
    double realizedPnL{};
    double unrealizedPnL{};
    double lastPrice{};
    double entryFees{};       // Fees attributed to the currently open quantity
};
#endif // POSITION_H
