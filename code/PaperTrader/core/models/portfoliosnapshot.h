#pragma once

struct PortfolioSnapshot {
    double accountBalance = 0.0;   // Cash on hand
    double equity = 0.0;           // Cash + market value of open positions
    double realizedPnL = 0.0;      // Closed trade profit/loss
    double unrealizedPnL = 0.0;    // Open trade profit/loss
    double accountMargin = 0.0;    // Margin consumed by open positions
    double availableFunds = 0.0;   // Equity - margins
    double orderMargin = 0.0;      // Margin reserved for pending orders
};
