#ifndef PREFIXES_H
#define PREFIXES_H

#include <QCoreApplication>

#include "CoreExport.h"
#include <QString>
#include <QDate>

class QWidget;
class QSqlDatabase;

/**
 * Global application constants used by all apps
 */
namespace AppConstants {
// Currency & Formatting: loaded at startup from the shop's settings by
// AppHelpers::loadCurrencyConfig(). Defined once in Core, so every app and
// Core itself read the same values.
CORE_EXPORT extern const char* CURRENCY;
CORE_EXPORT extern int PRICE_DECIMALS;

// Date/Time Formats
constexpr const char* DATETIME_DISPLAY = "dd/MM/yyyy HH:mm";

// UI Timing
constexpr int SEARCH_DELAY_MS = 300;

// Common Messages
constexpr const char* MSG_SAVE_SUCCESS = QT_TRANSLATE_NOOP("Messages", "Saved successfully");
constexpr const char* MSG_DELETE_SUCCESS = QT_TRANSLATE_NOOP("Messages", "Deleted successfully");
constexpr const char* MSG_DELETE_CONFIRM = QT_TRANSLATE_NOOP("Messages", "Are you sure you want to delete this item?");
constexpr const char* MSG_NO_PERMISSION = QT_TRANSLATE_NOOP("Messages", "You don't have permission to perform this action");
}

/**
 * Global helper functions
 */
namespace AppHelpers {
/**
     * A shared message (the *Constants.h MSG_ values) in the app language.
     */
inline QString trMsg(const char* message)
{
    return QCoreApplication::translate("Messages", message);
}

/**
     * A role ("Admin", "Manager", "Cashier") as shown to the user.
     */
CORE_EXPORT QString roleName(const QString& role);

/**
     * Load the currency code and its decimal places from app_settings.
     * Called at app startup and after the currency changes in Settings.
     */
CORE_EXPORT void loadCurrencyConfig(QSqlDatabase db);

/**
     * Format price with currency symbol
     * Example: 12.345 → "12.345 TND" (currency and decimals from the settings)
     */
QString formatPrice(double price);

/**
     * Format an amount with thousands separators and no currency
     * Example: 1234.5 → "1,234.500"
     */
QString formatAmount(double amount);

/**
     * Format date for display
     * Example: 2024-01-15 → "15/01/2024"
     */
QString formatDate(const QDate& date);

/**
     * Show confirmation dialog
     * Returns true if user clicked Yes
     */
bool confirmAction(QWidget* parent, const QString& message,
                   const QString& title = "Confirm");

/**
     * Show info message
     */
void showInfo(QWidget* parent, const QString& message,
              const QString& title = "Done");

/**
     * Show warning message
     */
void showWarning(QWidget* parent, const QString& message,
                 const QString& title = "Check this");

/**
     * Show error message
     */
void showError(QWidget* parent, const QString& message,
               const QString& title = "Something went wrong");

/**
     * Generate unique ticket number
     */
QString generateTicketNumber();

/**
 * Resolve the executable path of a sibling application (e.g. "Register").
 * Tries the current dir, sibling subdirs and ../../ at different depths,
 * including qmake's debug/ and release/ output subdirs.
 * Returns an empty string if not found.
 */
QString findSiblingPath(const QString& appName);

/**
 * Bring a window to the front without un-maximizing it.
 * Un-minimizes it if needed, then raises and activates it.
 * Never changes the size of an already-visible window.
 */
void bringToFront(QWidget* w);
}

#endif // PREFIXES_H
