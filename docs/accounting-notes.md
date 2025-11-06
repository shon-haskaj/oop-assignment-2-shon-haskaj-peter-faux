# Trading Simulator Accounting Notes

## Formulas
- **Account Balance**: cash after fills and fees. Buying reduces cash by `price × qty + fee`. Selling long increases cash by `price × qty − fee`. Short sales do not credit proceeds; proceeds remain locked as short collateral while cash only pays fees.
- **Equity**: `cash + unrealized P&L`.
- **Unrealized P&L**:
  - Long: `qty × (last_price − average_price)`.
  - Short: `|qty| × (average_price − last_price)`.
- **Realized P&L**:
  - Closing long: `(sell_price − avg_long_price) × closed_qty − fee_share`.
  - Covering short: `(avg_short_price − buy_price) × closed_qty − fee_share`.
- **Account Margin**: `|qty| × last_price × short_margin_rate` for open shorts (spot longs require no margin). Default `short_margin_rate = 0.5`.
- **Order Margin**: reserved funds for the opening side of pending orders (buys reserve cost + fees, shorts reserve `|qty| × price × short_margin_rate`).
- **Available Funds**: `max(0, cash − account_margin − order_margin)` recalculated on each fill and price update.

## Validation Highlights
- Quantities must be positive; limit prices must be > 0.
- Side flips close the current exposure before validating the new direction so we never double-count risk.
- Opening trades require sufficient buying power. Insufficient capacity triggers `ERR_INSUFFICIENT_FUNDS`, `ERR_INSUFFICIENT_MARGIN`, or `ERR_PARTIAL_FILL`.
- Fees are charged immediately and attributed to realized P&L for closed quantities.
- Short proceeds stay in `Position::shortCollateral` and are released when covering.

## Running the Generated Tests
The QtTest suite lives in `code/PaperTrader/tests/test_tradinglogic.cpp`. To execute it manually:

```bash
cd code/PaperTrader/tests
# Example with Qt 6 (update pkg-config names for Qt 5 if required)
g++ -std=c++17 \
    ../core/ordermanager.cpp ../core/portfoliomanager.cpp test_tradinglogic.cpp \
    -I../core -I../core/models \
    $(pkg-config --cflags --libs Qt6Core Qt6Test) -o tradingtests
./tradingtests
```

The binary exercises market and limit orders, side flips, margin validation, and fee edge cases without launching the UI.
