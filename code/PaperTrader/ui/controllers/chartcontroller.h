#pragma once
#include <QObject>
class ChartController : public QObject {
    Q_OBJECT
public:
    explicit ChartController(QObject *parent = nullptr);
};
