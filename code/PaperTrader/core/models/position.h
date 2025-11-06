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
};
#endif // POSITION_H
