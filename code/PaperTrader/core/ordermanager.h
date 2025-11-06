#pragma once
#include <QObject>
#include "models/order.h"

class OrderManager : public QObject {
    Q_OBJECT
public:
    explicit OrderManager(QObject *parent = nullptr);
};
