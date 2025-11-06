#ifndef POSITION_H
#define POSITION_H
#include <QString>

struct Position {
    QString symbol;
    int qty{};
    double avgPx{};
    double realizedPnL{};
    double unrealizedPnL{};
};
#endif // POSITION_H
