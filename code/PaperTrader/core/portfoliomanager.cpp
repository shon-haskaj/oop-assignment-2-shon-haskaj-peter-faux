#include "portfoliomanager.h"

#include <QtGlobal>
#include <cmath>
#include <algorithm>
#include <limits>

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
    const double available = availableFundsInternalWithoutClamp();
    // Available funds are clamped for display only – internal validation uses
    // the raw value so risk checks can detect deficits accurately.
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
    const double unitFee = perUnitFee(effectivePrice);

    double closingTarget = 0.0;
    if (isBuy && positionQty < 0.0) {
        closingTarget = std::min(quantity, std::abs(positionQty));
    } else if (isSell && positionQty > 0.0) {
        closingTarget = std::min(quantity, positionQty);
    }

    double closingQty = closingTarget;
    bool closingLimited = false;
    if (isBuy && closingQty > 0.0) {
        const double perUnitCash = effectivePrice + unitFee;
        if (perUnitCash > 0.0) {
            const double maxClosable = std::min(closingQty, m_cash / perUnitCash);
            if (maxClosable + 1e-9 < closingQty) {
                closingQty = maxClosable;
                closingLimited = true;
            }
        }
    }

    double openingQtyRequested = std::max(0.0, quantity - closingTarget);
    if (closingLimited)
        openingQtyRequested = 0.0;

    if (closingLimited && closingQty <= 0.0) {
        result.errorCode = QStringLiteral("ERR_INSUFFICIENT_FUNDS");
        return result;
    }

    double available = availableFundsInternalWithoutClamp();
    double cashAfterClosing = m_cash;

    if (closingQty > 0.0) {
        if (isBuy) {
            const double marginRelease = effectivePrice * shortMarginRate() * closingQty;
            const double cashChange = -closingQty * (effectivePrice + unitFee);
            available += marginRelease + cashChange;
            cashAfterClosing += cashChange;
        } else {
            const double marginRelease = effectivePrice * longMarginRate() * closingQty;
            const double cashChange = closingQty * (effectivePrice - unitFee);
            available += marginRelease + cashChange;
            cashAfterClosing += cashChange;
        }
    }

    double acceptedOpening = 0.0;
    QString openingError;
    double requiredMarginForOpening = 0.0;

    if (openingQtyRequested > 0.0) {
        if (isBuy) {
            const double perUnitCost = effectivePrice;
            const double perUnitMargin = effectivePrice * longMarginRate();
            const double perUnitCashNeed = perUnitCost + unitFee;
            const double perUnitRequirement = perUnitCashNeed + perUnitMargin;
            const double requiredCash = openingQtyRequested * perUnitCashNeed;
            const double requiredFunds = requiredCash + openingQtyRequested * perUnitMargin;

            if (requiredFunds <= available + 1e-9 && requiredCash <= cashAfterClosing + 1e-9) {
                acceptedOpening = openingQtyRequested;
                requiredMarginForOpening = acceptedOpening * perUnitMargin;
            } else {
                const double maxByAvailable = perUnitRequirement > 0.0
                        ? available / perUnitRequirement
                        : 0.0;
                const double maxByCash = perUnitCashNeed > 0.0
                        ? cashAfterClosing / perUnitCashNeed
                        : 0.0;
                const double maxQty = std::max(0.0, std::min({openingQtyRequested, maxByAvailable, maxByCash}));
                if (maxQty > 0.0) {
                    acceptedOpening = maxQty;
                    requiredMarginForOpening = acceptedOpening * perUnitMargin;
                    openingError = QStringLiteral("ERR_PARTIAL_FILL");
                } else {
                    openingError = QStringLiteral("ERR_INSUFFICIENT_FUNDS");
                }
            }
        } else {
            const double perUnitMargin = effectivePrice * shortMarginRate();
            const double perUnitCashNeed = unitFee;
            const double perUnitRequirement = perUnitMargin + perUnitCashNeed;
            const double requiredCash = openingQtyRequested * perUnitCashNeed;
            const double requiredFunds = requiredCash + openingQtyRequested * perUnitMargin;

            if (requiredFunds <= available + 1e-9 && requiredCash <= cashAfterClosing + 1e-9) {
                acceptedOpening = openingQtyRequested;
                requiredMarginForOpening = acceptedOpening * perUnitMargin;
            } else {
                const double maxByAvailable = perUnitRequirement > 0.0
                        ? available / perUnitRequirement
                        : 0.0;
                const double maxByCash = perUnitCashNeed > 0.0
                        ? cashAfterClosing / perUnitCashNeed
                        : std::numeric_limits<double>::infinity();
                const double maxQty = std::max(0.0, std::min({openingQtyRequested, maxByAvailable, maxByCash}));
                if (maxQty > 0.0) {
                    acceptedOpening = maxQty;
                    requiredMarginForOpening = acceptedOpening * perUnitMargin;
                    openingError = QStringLiteral("ERR_PARTIAL_FILL");
                } else {
                    openingError = QStringLiteral("ERR_INSUFFICIENT_MARGIN");
                }
            }
        }
    }

    const double acceptedQuantity = closingQty + acceptedOpening;
    if (acceptedQuantity <= 0.0) {
        result.errorCode = openingError.isEmpty()
                ? QStringLiteral("ERR_REJECTED")
                : openingError;
        return result;
    }

    result.accepted = true;
    result.acceptedQuantity = acceptedQuantity;
    result.closingQuantity = closingQty;
    result.openingQuantity = acceptedOpening;
    result.partial = std::abs(acceptedQuantity - quantity) > 1e-9;
    result.estimatedFees = unitFee * acceptedQuantity;
    result.fee = result.estimatedFees;
    result.estimatedOrderMargin = requiredMarginForOpening;
    if (!openingError.isEmpty() || (closingQty + acceptedOpening) < quantity) {
        result.errorCode = openingError.isEmpty()
                ? QStringLiteral("ERR_PARTIAL_FILL")
                : openingError;
    }
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

    const double filledQty = order.filledQuantity;
    if (price <= 0.0 || filledQty <= 0.0)
        return;

    const double totalFee = order.fee > 0.0
            ? order.fee
            : estimateFee(price, filledQty);
    const double feePerUnit = filledQty > 0.0 ? totalFee / filledQty : 0.0;

    double remaining = filledQty;

    auto proportionalEntryFee = [](double totalEntryFees, double positionQty, double closingQty) {
        const double absQty = std::abs(positionQty);
        if (absQty <= 0.0)
            return 0.0;
        return totalEntryFees * (closingQty / absQty);
    };

    auto weightedAverage = [](double existingQty, double existingAvg, double addQty, double addPrice) {
        const double absExisting = std::abs(existingQty);
        const double total = absExisting + addQty;
        if (total <= 0.0)
            return addPrice;
        return (existingAvg * absExisting + addQty * addPrice) / total;
    };

    if (isBuy) {
        if (pos.qty < 0.0) {
            const double coverQty = std::min(remaining, -pos.qty);
            if (coverQty > 0.0) {
                const double entryFeeShare = proportionalEntryFee(pos.entryFees, pos.qty, coverQty);
                const double realized = (pos.avgPx - price) * coverQty - entryFeeShare - feePerUnit * coverQty;
                m_cash -= price * coverQty;
                pos.qty += coverQty;
                pos.entryFees -= entryFeeShare;
                pos.realizedPnL += realized;
                m_realizedPnL += realized;
                remaining -= coverQty;
                if (std::abs(pos.entryFees) < 1e-9)
                    pos.entryFees = 0.0;
                if (qFuzzyIsNull(pos.qty)) {
                    pos.qty = 0.0;
                    pos.avgPx = 0.0;
                    pos.entryFees = 0.0;
                }
            }
        }

        if (remaining > 0.0) {
            m_cash -= price * remaining;
            pos.avgPx = weightedAverage(pos.qty, pos.avgPx, remaining, price);
            pos.qty += remaining;
            pos.entryFees += feePerUnit * remaining;
            remaining = 0.0;
        }
    } else {
        if (pos.qty > 0.0) {
            const double sellQty = std::min(remaining, pos.qty);
            if (sellQty > 0.0) {
                const double entryFeeShare = proportionalEntryFee(pos.entryFees, pos.qty, sellQty);
                const double realized = (price - pos.avgPx) * sellQty - entryFeeShare - feePerUnit * sellQty;
                m_cash += price * sellQty;
                pos.qty -= sellQty;
                pos.entryFees -= entryFeeShare;
                pos.realizedPnL += realized;
                m_realizedPnL += realized;
                remaining -= sellQty;
                if (std::abs(pos.entryFees) < 1e-9)
                    pos.entryFees = 0.0;
                if (qFuzzyIsNull(pos.qty)) {
                    pos.qty = 0.0;
                    pos.avgPx = 0.0;
                    pos.entryFees = 0.0;
                }
            }
        }

        if (remaining > 0.0) {
            pos.avgPx = weightedAverage(pos.qty, pos.avgPx, remaining, price);
            pos.qty -= remaining;
            pos.entryFees += feePerUnit * remaining;
            remaining = 0.0;
        }
    }

    m_cash -= totalFee;
    if (m_cash < 0.0 && m_cash > -1e-8)
        m_cash = 0.0;

    m_lastPrices[symbol] = price;
    pos.lastPrice = price;
    updateUnrealized(pos);

    recordOrUpdatePosition(symbol, pos);
    recomputeOrderMargin();
    emitSnapshot();
}

void PortfolioManager::onOrdersUpdated
void PortfolioManager::onOrdersUpdated(const QList<Order> &orders)
{
    m_openOrders.clear();
    for (const Order &order : orders) {
        const double remaining = remainingOrderQuantity(order);
        const bool isCancelled = order.status.compare(QStringLiteral("Cancelled"), Qt::CaseInsensitive) == 0;
        const bool isRejected = order.status.compare(QStringLiteral("Rejected"), Qt::CaseInsensitive) == 0;
        const bool isFilled = order.status.compare(QStringLiteral("Filled"), Qt::CaseInsensitive) == 0;
        if (remaining <= 0.0 || isCancelled || isRejected || isFilled)
            continue;
        m_openOrders.append(order);
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
    pos.symbol = symbol;
    pos.lastPrice = m_lastPrices.value(symbol, pos.lastPrice > 0.0 ? pos.lastPrice : pos.avgPx);
    updateUnrealized(pos);
    m_positions.insert(symbol, pos);
}

void PortfolioManager::updateUnrealized(Position &position)
{
    const double last = position.lastPrice > 0.0
            ? position.lastPrice
            : m_lastPrices.value(position.symbol, position.avgPx);
    position.lastPrice = last > 0.0 ? last : position.avgPx;
    if (position.qty > 0.0) {
        position.unrealizedPnL = position.qty * (position.lastPrice - position.avgPx);
    } else if (position.qty < 0.0) {
        position.unrealizedPnL = std::abs(position.qty) * (position.avgPx - position.lastPrice);
    } else {
        position.unrealizedPnL = 0.0;
    }
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
    const double remaining = remainingOrderQuantity(order);
    if (remaining <= 0.0)
        return 0.0;
    double price = order.price;
    if (price <= 0.0)
        price = order.filledPrice;
    if (price <= 0.0)
        price = m_lastPrices.value(symbol, 0.0);
    return marginForOrder(symbol, order.side, remaining, price);
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

double PortfolioManager::availableFundsInternalWithoutClamp() const
{
    double margin = 0.0;
    for (const Position &pos : m_positions) {
        margin += marginForPosition(pos);
    }
    return m_cash - margin - m_orderMargin;
}

double PortfolioManager::perUnitFee(double price) const
{
    return std::abs(price) * m_feeRate;
}

double PortfolioManager::remainingOrderQuantity(const Order &order) const
{
    const double remaining = order.quantity - order.filledQuantity;
    return remaining > 0.0 ? remaining : 0.0;
}

double PortfolioManager::availableFundsInternal() const
{
    const double available = availableFundsInternalWithoutClamp();
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
