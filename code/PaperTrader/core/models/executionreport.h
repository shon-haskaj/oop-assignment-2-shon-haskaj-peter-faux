#ifndef EXECUTIONREPORT_H
#define EXECUTIONREPORT_H
#include <QString>

struct ExecutionReport {
    int orderId{};
    QString status;   // "Filled", "Cancelled"
    double fillPx{};
    int fillQty{};
};
#endif // EXECUTIONREPORT_H
