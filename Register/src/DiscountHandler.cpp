#include "DiscountHandler.h"
#include <QtMath>

double DiscountHandler::calculateAmount(const DiscountItem& discount, double subtotal)
{
    if (discount.type == "percentage") {
        // Percentage discount
        return subtotal * (discount.value / 100.0);
    } else {
        // Fixed amount discount - don't exceed subtotal
        return qMin(discount.value, subtotal);
    }
}
