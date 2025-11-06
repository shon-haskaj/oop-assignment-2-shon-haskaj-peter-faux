#pragma once
#include <QObject>

class ChartManager : public QObject {
    Q_OBJECT
public:
    explicit ChartManager(QObject *parent = nullptr);
};
