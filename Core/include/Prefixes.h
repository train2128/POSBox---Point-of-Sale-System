#ifndef PREFIXES_H
#define PREFIXES_H

#include <QString>
#include <QDate>
#include <QDateTime>

class QWidget;

/**
 * Global application constants used by all apps
 */
namespace AppConstants {
// Currency & Formatting
constexpr const char* CURRENCY = "TND";
constexpr int PRICE_DECIMALS = 3;

// Date/Time Formats
constexpr const char* DATE_FORMAT = "yyyy-MM-dd";
constexpr const char* TIME_FORMAT = "HH:mm:ss";
constexpr const char* DATETIME_DISPLAY = "dd/MM/yyyy HH:mm";
constexpr const char* DATETIME_FILE = "yyyyMMdd_HHmmss";

// Cart Key System (Used by all POS apps)
constexpr int CART_KEY_VARIANT_OFFSET = 1000000;
constexpr int CART_KEY_CUSTOM_OFFSET = -1000000;

// UI Timing
constexpr int SEARCH_DELAY_MS = 300;
constexpr int NOTIFICATION_CHECK_INTERVAL = 360000; // 6 minutes

// UI Sizes
constexpr int DEFAULT_BUTTON_HEIGHT = 35;
constexpr int DEFAULT_ICON_SIZE = 24;

// Common Messages
constexpr const char* MSG_DATABASE_ERROR = "Database connection error";
constexpr const char* MSG_SAVE_SUCCESS = "Saved successfully";
constexpr const char* MSG_DELETE_SUCCESS = "Deleted successfully";
constexpr const char* MSG_UPDATE_SUCCESS = "Updated successfully";
constexpr const char* MSG_DELETE_CONFIRM = "Are you sure you want to delete this item?";
constexpr const char* MSG_NO_PERMISSION = "You don't have permission to perform this action";
constexpr const char* MSG_FEATURE_COMING_SOON = "This feature is coming soon!";
}

/**
 * Global helper functions
 */
namespace AppHelpers {
/**
     * Format price with currency symbol
     * Example: 12.345 → "12.345 TND"
     */
QString formatPrice(double price);

/**
     * Format date for display
     * Example: 2024-01-15 → "15/01/2024"
     */
QString formatDate(const QDate& date);

/**
     * Format datetime for display
     * Example: "15/01/2024 14:30"
     */
QString formatDateTime(const QDateTime& dt);

/**
     * Format datetime for filenames
     * Example: "20240115_143045"
     */
QString formatDateTimeFile(const QDateTime& dt);

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
              const QString& title = "Information");

/**
     * Show warning message
     */
void showWarning(QWidget* parent, const QString& message,
                 const QString& title = "Warning");

/**
     * Show error message
     */
void showError(QWidget* parent, const QString& message,
               const QString& title = "Error");

/**
     * Generate unique ticket number
     */
QString generateTicketNumber();
}

#endif // PREFIXES_H
