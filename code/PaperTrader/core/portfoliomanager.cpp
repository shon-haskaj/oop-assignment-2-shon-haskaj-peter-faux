#include "portfoliomanager.h"

#include <QtGlobal>
#include <cmath>
#include <algorithm>

PortfolioManager::PortfolioManager(QObject *parent)
    : QObject(parent) {}

double PortfolioManager::totalUnrealizedPnL() const
{
    double total = 0.0;
    for (const Position &pos : m_positions) {
        const double last = pos.lastPrice > 0.0
                                ? pos.lastPrice
                                : m_lastPrices.value(pos.symbol, pos.avgPx);
        if (pos.qty > 0.0) {
            total += pos.qty * (last - pos.avgPx);
        } else if (pos.qty < 0.0) {
            total += std::abs(pos.qty) * (pos.avgPx - last);
        }
    }
    return total;
}

PortfolioSnapshot PortfolioManager::snapshot() const
{
    PortfolioSnapshot snap;
    snap.accountBalance = m_cash;
    snap.realizedPnL = m_realizedPnL;
    snap.unrealizedPnL = totalUnrealizedPnL();

    double margin = 0.0;
    for (const Position &pos : m_positions) {
        margin += marginForPosition(pos);
    }

    snap.equity = snap.accountBalance + snap.unrealizedPnL;
    snap.accountMargin = margin;
    snap.orderMargin = m_orderMargin;
    const double available = m_cash - snap.accountMargin - snap.orderMargin;
    // Available funds are clamped so we never report negative buying power.
    snap.availableFunds = available > 0.0 ? available : 0.0;
    return snap;
}

double PortfolioManager::estimateFee(double price, double quantity) const
{
    return std::abs(price * quantity) * m_feeRate;
}

PortfolioManager::OrderValidationResult PortfolioManager::validateOrder(
    bool isMarket,
    const QString &symbol,
    const QString &side,
    double quantity,
    double price) const
{
    OrderValidationResult result;
    const QString normalisedSymbol = symbol.trimmed().toUpper();
    if (normalisedSymbol.isEmpty()) {
        result.errorCode = QStringLiteral("ERR_INVALID_SYMBOL");
        return result;
    }

    if (quantity <= 0.0) {
        result.errorCode = QStringLiteral("ERR_INVALID_QTY");
        return result;
    }

    const bool isBuy = side.compare(QStringLiteral("BUY"), Qt::CaseInsensitive) == 0;
    const bool isSell = side.compare(QStringLiteral("SELL"), Qt::CaseInsensitive) == 0;
    if (!isBuy && !isSell) {
        result.errorCode = QStringLiteral("ERR_INVALID_SIDE");
        return result;
    }

    double effectivePrice = price;
    if (!isMarket && effectivePrice <= 0.0) {
        result.errorCode = QStringLiteral("ERR_INVALID_PRICE");
        return result;
    }

    if (effectivePrice <= 0.0) {
        effectivePrice = m_lastPrices.value(normalisedSymbol, 0.0);
    }

    if (effectivePrice <= 0.0) {
        result.errorCode = QStringLiteral("ERR_INVALID_PRICE");
        return result;
    }

    result.effectivePrice = effectivePrice;

    const Position pos = m_positions.value(normalisedSymbol);
    const double positionQty = pos.qty;
    double closingQty = 0.0;
    double openingQty = quantity;

    // Side flips close the existing exposure first before we validate the new
    // direction so that margin checks never double-count risk.
    if (isBuy && positionQty < 0.0) {
        closingQty = std::min(quantity, std::abs(positionQty));
        openingQty = quantity - closingQty;
    } else if (isSell && positionQty > 0.0) {
        closingQty = std::min(quantity, positionQty);
        openingQty = quantity - closingQty;
    }

    const double closingFee = estimateFee(effectivePrice, closingQty);
    const double openingFee = estimateFee(effectivePrice, openingQty);
    const double totalFee = closingFee + openingFee;

    if (m_cash < totalFee) {
        result.errorCode = QStringLiteral("ERR_INSUFFICIENT_FUNDS");
        return result;
    }

    // Available funds = cash - margin - reserved order margin. When closing we
    // simulate releasing resources before validating any new exposure.
    double available = availableFundsInternal();

    if (closingQty > 0.0) {
        if (isBuy) {
            const double absPos = std::abs(positionQty);
            const double collateralPerUnit = absPos > 0.0
                                                 ? pos.shortCollateral / absPos
                                                 : 0.0;
            const double release = collateralPerUnit * closingQty;
            const double cost = closingQty * effectivePrice + closingFee;
            available += release - cost;
        } else {
            const double proceeds = closingQty * effectivePrice - closingFee;
            available += proceeds;
        }
        available = std::max(available, 0.0);
    }

    double acceptedOpening = 0.0;
    QString openingError;

    if (openingQty > 0.0) {
        if (isBuy) {
            const double perUnitCost = effectivePrice * (1.0 + m_feeRate);
            const double required = openingQty * perUnitCost;
            if (available >= required) {
                acceptedOpening = openingQty;
            } else {
                const double maxQty = available / perUnitCost;
                if (maxQty > 0.0) {
                    acceptedOpening = std::min(openingQty, maxQty);
                    openingError = QStringLiteral("ERR_PARTIAL_FILL");
                } else {
                    openingError = QStringLiteral("ERR_INSUFFICIENT_FUNDS");
                }
            }
        } else {
            const double perUnitMargin = effectivePrice * m_shortMarginRate;
            const double requiredMargin = openingQty * perUnitMargin;
            if (available >= requiredMargin) {
                acceptedOpening = openingQty;
            } else {
                const double maxQty = perUnitMargin > 0.0
                                          ? available / perUnitMargin
                                          : 0.0;
                if (maxQty > 0.0) {
                    acceptedOpening = std::min(openingQty, maxQty);
                    openingError = QStringLiteral("ERR_PARTIAL_FILL");
                } else {
                    openingError = QStringLiteral("ERR_INSUFFICIENT_MARGIN");
                }
            }
        }
    }

    result.acceptedQuantity = closingQty + acceptedOpening;
    result.partial = std::abs(result.acceptedQuantity - quantity) > 1e-9;

    if (result.acceptedQuantity <= 0.0) {
        result.errorCode = openingError.isEmpty()
        ? QStringLiteral("ERR_INSUFFICIENT_FUNDS")
        : openingError;
        return result;
    }

    result.accepted = true;
    if (!openingError.isEmpty()) {
        result.errorCode = openingError;
    }
    result.fee = estimateFee(effectivePrice, result.acceptedQuantity);
    return result;
}

void PortfolioManager::onCandle(const Candle &c)
{
    const QString symbol = c.symbol.toUpper();
    m_lastPrices[symbol] = c.close;
    updateUnrealizedFor(symbol);
    emitSnapshot();
}

void PortfolioManager::applyFill(const Order &order)
{
    if (order.filledQuantity <= 0.0)
        return;

    const QString symbol = order.symbol.toUpper();
    Position pos = m_positions.value(symbol);
    pos.symbol = symbol;

    const bool isBuy = order.side.compare("BUY", Qt::CaseInsensitive) == 0;
    double price = order.filledPrice > 0.0
                       ? order.filledPrice
                       : m_lastPrices.value(symbol, order.price);
    if (price <= 0.0)
        price = order.price;
    if (price <= 0.0)
        price = m_lastPrices.value(symbol, 0.0);

    // Total fee actually charged for this fill (may be provided by venue).
    double totalFee = order.fee > 0.0
                          ? order.fee
                          : estimateFee(price, order.filledQuantity);

    auto weightedAverage = [](double existingQty, double existingAvg, double addQty, double addPrice) {
        const double absExisting = std::abs(existingQty);
        const double total = absExisting + addQty;
        if (qFuzzyIsNull(total))
            return 0.0;
        return (existingAvg * absExisting + addPrice * addQty) / total;
    };

    double remainingQty = order.filledQuantity;

    if (isBuy) {
        while (remainingQty > 0.0) {
            if (pos.qty < 0.0) {
                const double coverQty = std::min(remainingQty, -pos.qty);
                const double absPos = std::abs(pos.qty);
                const double collateralPerUnit = absPos > 0.0
                                                     ? pos.shortCollateral / absPos
                                                     : 0.0;
                const double collateralRelease = collateralPerUnit * coverQty;
                const double realized = (pos.avgPx - price) * coverQty;

                // Release collateral then pay to cover the borrowed shares.
                m_cash += collateralRelease;
                m_cash -= price * coverQty;

                pos.shortCollateral -= collateralRelease;
                pos.realizedPnL += realized;
                m_realizedPnL += realized;

                pos.qty += coverQty;
                remainingQty -= coverQty;
                if (qFuzzyIsNull(pos.qty))
                    pos.avgPx = 0.0;
            } else {
                // Opening / adding to a long position consumes cash.
                m_cash -= price * remainingQty;
                pos.avgPx = weightedAverage(pos.qty, pos.avgPx, remainingQty, price);
                pos.qty += remainingQty;
                remainingQty = 0.0;
            }
        }
    } else {
        while (remainingQty > 0.0) {
            if (pos.qty > 0.0) {
                const double sellQty = std::min(remainingQty, pos.qty);
                const double realized = (price - pos.avgPx) * sellQty;

                // Closing a long: proceeds go straight to cash.
                m_cash += price * sellQty;

                pos.realizedPnL += realized;
                m_realizedPnL += realized;

                pos.qty -= sellQty;
                remainingQty -= sellQty;
                if (qFuzzyIsNull(pos.qty))
                    pos.avgPx = 0.0;
            } else {
                // New short: proceeds are locked as collateral.
                pos.shortCollateral += price * remainingQty;
                pos.avgPx = weightedAverage(pos.qty, pos.avgPx, remainingQty, price);
                pos.qty -= remainingQty;
                remainingQty = 0.0;
            }
        }
    }

    // Charge the full fee to cash once per order fill...
    m_cash -= totalFee;
    if (m_cash < 0.0 && m_cash > -1e-8)
        m_cash = 0.0;

    // ...and reflect the exact same expense in realized P&L.
    pos.realizedPnL -= totalFee;
    m_realizedPnL -= totalFee;

    m_lastPrices[symbol] = price;
    pos.lastPrice = m_lastPrices.value(symbol, price);
    if (pos.qty > 0.0) {
        pos.unrealizedPnL = pos.qty * (pos.lastPrice - pos.avgPx);
    } else if (pos.qty < 0.0) {
        pos.unrealizedPnL = std::abs(pos.qty) * (pos.avgPx - pos.lastPrice);
    } else {
        pos.unrealizedPnL = 0.0;
        pos.shortCollateral = 0.0;
    }

    recordOrUpdatePosition(symbol, pos);

    emitSnapshot();
}

void PortfolioManager::onOrdersUpdated(const QList<Order> &orders)
{
    m_openOrders.clear();
    for (const Order &order : orders) {
        const bool isOpen = order.status.compare(QStringLiteral("Open"), Qt::CaseInsensitive) == 0;
        const bool isPartial = order.status.compare(QStringLiteral("PartiallyFilled"), Qt::CaseInsensitive) == 0;
        if ((isOpen || isPartial) && order.quantity > 0.0) {
            m_openOrders.append(order);
        }
    }
    // Pending orders reserve margin so that available funds reflect true buying power.
    recomputeOrderMargin();
    emitSnapshot();
}

void PortfolioManager::updateUnrealizedFor(const QString &symbol)
{
    if (!m_positions.contains(symbol))
        return;

    Position pos = m_positions.value(symbol);
    pos.lastPrice = m_lastPrices.value(symbol, pos.avgPx);
    if (pos.qty > 0.0) {
        pos.unrealizedPnL = pos.qty * (pos.lastPrice - pos.avgPx);
    } else if (pos.qty < 0.0) {
        pos.unrealizedPnL = std::abs(pos.qty) * (pos.avgPx - pos.lastPrice);
    } else {
        pos.unrealizedPnL = 0.0;
    }
    m_positions.insert(symbol, pos);
}

double PortfolioManager::marginForPosition(const Position &position) const
{
    if (position.qty >= 0.0)
        return 0.0;

    const double last = position.lastPrice > 0.0
                            ? position.lastPrice
                            : m_lastPrices.value(position.symbol, position.avgPx);
    const double notional = std::abs(position.qty) * last;
    // Short margin requirement is configurable so risk can be tuned per venue.
    return notional * m_shortMarginRate;
}

double PortfolioManager::marginForOrder(const Order &order) const
{
    const QString symbol = order.symbol.toUpper();
    const double price = order.price > 0.0
                             ? order.price
                             : m_lastPrices.value(symbol, 0.0);
    return marginForOrder(symbol, order.side, order.quantity, price);
}

double PortfolioManager::marginForOrder(const QString &symbol,
                                        const QString &side,
                                        double quantity,
                                        double price) const
{
    if (quantity <= 0.0 || price <= 0.0)
        return 0.0;

    const double openingQty = openingQuantityForOrder(symbol, side, quantity);
    if (openingQty <= 0.0)
        return 0.0;

    const bool isBuy = side.compare(QStringLiteral("BUY"), Qt::CaseInsensitive) == 0;
    if (isBuy) {
        const double perUnitCost = price * (1.0 + m_feeRate);
        return openingQty * perUnitCost;
    }

    const double notional = openingQty * price;
    return notional * m_shortMarginRate;
}

double PortfolioManager::openingQuantityForOrder(const QString &symbol,
                                                 const QString &side,
                                                 double quantity) const
{
    const Position pos = m_positions.value(symbol.toUpper());
    if (quantity <= 0.0)
        return 0.0;

    if (side.compare(QStringLiteral("BUY"), Qt::CaseInsensitive) == 0) {
        if (pos.qty < 0.0) {
            const double closing = std::min(quantity, std::abs(pos.qty));
            return quantity - closing;
        }
        return quantity;
    }

    if (pos.qty > 0.0) {
        const double closing = std::min(quantity, pos.qty);
        return quantity - closing;
    }
    return quantity;
}

double PortfolioManager::availableFundsInternal() const
{
    double margin = 0.0;
    for (const Position &pos : m_positions) {
        margin += marginForPosition(pos);
    }
    const double available = m_cash - margin - m_orderMargin;
    return available > 0.0 ? available : 0.0;
}

void PortfolioManager::recomputeOrderMargin()
{
    double margin = 0.0;
    for (const Order &order : m_openOrders) {
        margin += marginForOrder(order);
    }
    m_orderMargin = margin;
}

void PortfolioManager::recordOrUpdatePosition(const QString &symbol, const Position &position)
{
    if (qFuzzyIsNull(position.qty)) {
        m_positions.remove(symbol);
    } else {
        Position copy = position;
        copy.symbol = symbol;
        m_positions.insert(symbol, copy);
    }
}

void PortfolioManager::emitSnapshot()
{
    emit portfolioChanged(snapshot(), positions());
}
