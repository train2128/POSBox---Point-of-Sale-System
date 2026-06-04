#ifndef DISCOUNTHANDLER_H
#define DISCOUNTHANDLER_H

#include <QString>

/**
 * Discount data structure
 */
struct DiscountItem {
    QString name;
    QString type;  // "percentage" or "amount"
    double value;
    int presetId;

    DiscountItem() : value(0.0), presetId(-1) {}
};

/**
 * Handles discount calculations
 * Dialog integration will be added later when UI is created
 */
class DiscountHandler
{
public:
    /**
     * Calculate discount amount based on type
     * @param discount - Discount to apply
     * @param subtotal - Subtotal before discount
     * @return Amount to subtract from subtotal
     */
    static double calculateAmount(const DiscountItem& discount, double subtotal);

private:
    // No instances allowed
    DiscountHandler() = delete;
};

#endif // DISCOUNTHANDLER_H
