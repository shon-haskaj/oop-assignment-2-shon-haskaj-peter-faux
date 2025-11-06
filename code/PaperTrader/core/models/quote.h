#ifndef QUOTE_H
#define QUOTE_H
#include <QDateTime>

struct Quote {
    QDateTime timestamp;
    double bid{}, ask{}, last{};
};
#endif // QUOTE_H
