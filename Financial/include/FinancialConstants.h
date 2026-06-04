#ifndef FINANCIALCONSTANTS_H
#define FINANCIALCONSTANTS_H

namespace FinancialConstants {

// Tab indices
constexpr int TAB_OVERVIEW  = 0;
constexpr int TAB_YEARLY    = 1;

// Monthly summary table columns
constexpr int COL_MONTHLY_INCOME    = 0;
constexpr int COL_MONTHLY_COST      = 1;
constexpr int COL_MONTHLY_BENEFIT   = 2;
constexpr int COL_MONTHLY_EXPENSES  = 3;
constexpr int COL_MONTHLY_REST      = 4;

// Weekly table columns
constexpr int COL_WEEKLY_DAY        = 0;
constexpr int COL_WEEKLY_INCOME     = 1;
constexpr int COL_WEEKLY_EXPENSES   = 2;

// Top Sales table columns
constexpr int COL_TOPSALES_RANK     = 0;
constexpr int COL_TOPSALES_PRODUCT  = 1;
constexpr int COL_TOPSALES_QTY      = 2;

// One-time expenses table columns
constexpr int COL_EXP_NAME          = 0;
constexpr int COL_EXP_AMOUNT        = 1;
constexpr int COL_EXP_DATE          = 2;

// Yearly table columns
constexpr int COL_YEAR_MONTH        = 0;
constexpr int COL_YEAR_INCOME       = 1;
constexpr int COL_YEAR_COST         = 2;
constexpr int COL_YEAR_EXPENSES     = 3;
constexpr int COL_YEAR_BENEFIT      = 4;

// Messages
constexpr const char* MSG_NO_SELECTION      = "Please select an item first.";
constexpr const char* MSG_DELETE_CONFIRM     = "Are you sure you want to delete this item?";
constexpr const char* MSG_DELETE_SUCCESS     = "Deleted successfully.";

} // namespace FinancialConstants

#endif // FINANCIALCONSTANTS_H
