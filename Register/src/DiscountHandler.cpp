#include "DiscountHandler.h"
#include <QtMath>

double DiscountHandler::calculateAmount(const DiscountItem& discount, double subtotal)
{
    if (discount.type == "percentage") {
        // Percentage discount - never more than the whole sale
        return qMin(subtotal * (discount.value / 100.0), subtotal);
    } else {
        // Fixed amount discount - don't exceed subtotal
        return qMin(discount.value, subtotal);
    }
}
