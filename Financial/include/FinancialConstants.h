#ifndef FINANCIALCONSTANTS_H
#define FINANCIALCONSTANTS_H

#include <QCoreApplication>

namespace FinancialConstants {

// One-time expenses table columns
constexpr int COL_EXP_NAME          = 0;
constexpr int COL_EXP_AMOUNT        = 1;

// Yearly summary table columns
constexpr int COL_YEAR_MONTH        = 0;
constexpr int COL_YEAR_INCOME       = 1;
constexpr int COL_YEAR_COST         = 2;
constexpr int COL_YEAR_EXPENSES     = 3;
constexpr int COL_YEAR_BENEFIT      = 4;
constexpr int YEAR_COLUMNS          = 5;

// Messages
constexpr const char* MSG_NO_SELECTION      = QT_TRANSLATE_NOOP("Messages", "Please select an item first.");
constexpr const char* MSG_DELETE_CONFIRM    = QT_TRANSLATE_NOOP("Messages", "Are you sure you want to delete this item?");

} // namespace FinancialConstants

#endif // FINANCIALCONSTANTS_H
