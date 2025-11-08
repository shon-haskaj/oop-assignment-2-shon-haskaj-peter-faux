#ifndef QUOTE_H
#define QUOTE_H
#include <QDateTime>
#include <QMetaType>
#include <QString>

struct Quote {
    QString   symbol;
    QDateTime timestamp;
    double    bid{};
    double    ask{};
    double    last{};

    double mid() const
    {
        if (bid > 0.0 && ask > 0.0)
            return (bid + ask) / 2.0;
        return last;
    }

    bool isValid() const
    {
        return !symbol.isEmpty() && timestamp.isValid();
    }
};

Q_DECLARE_METATYPE(Quote)
#endif // QUOTE_H
