#pragma once
#include <QObject>

class PortfolioManager : public QObject {
    Q_OBJECT
public:
    explicit PortfolioManager(QObject *parent = nullptr);
};
