#ifndef SETTINGSCONSTANTS_H
#define SETTINGSCONSTANTS_H

#include <QCoreApplication>

#include "PrinterConstants.h"

/**
 * Settings-specific constants.
 * Printer defaults live in Core/PrinterConstants.h.
 * For global constants, see Core/CommonConfig.h
 */
namespace SettingsConstants {

// ============================================================================
// UI
// ============================================================================
constexpr int LOGO_IMAGE_SIZE = 110;

// ============================================================================
// LOCALE / VALIDATION
// These are defined here for easy future internationalization.
// In the future, these may be driven by a country selector in App Settings.
// ============================================================================

// ============================================================================
// MESSAGES - GENERAL
// ============================================================================
constexpr const char* MSG_ACCESS_DENIED  = QT_TRANSLATE_NOOP("Messages", "Access denied. Only Admins can access Settings.");
constexpr const char* MSG_SAVE_SUCCESS   = QT_TRANSLATE_NOOP("Messages", "Shop profile saved successfully.");
constexpr const char* MSG_SAVE_FAILED    = QT_TRANSLATE_NOOP("Messages", "Failed to save shop profile.");
constexpr const char* MSG_CANCEL_CONFIRM = QT_TRANSLATE_NOOP("Messages", "Discard unsaved changes?");

// ============================================================================
// MESSAGES - VALIDATION
// ============================================================================
constexpr const char* MSG_SHOP_NAME_EMPTY = QT_TRANSLATE_NOOP("Messages", "Shop name cannot be empty.");
constexpr const char* MSG_PHONE_INVALID   = QT_TRANSLATE_NOOP("Messages", "Phone number must contain exactly %1 digits after the prefix.");
constexpr const char* MSG_POSTAL_INVALID  = QT_TRANSLATE_NOOP("Messages", "Postal code must contain exactly %1 digits.");
constexpr const char* MSG_EMAIL_INVALID   = QT_TRANSLATE_NOOP("Messages", "Please enter a valid email address.");

// ============================================================================
// MESSAGES - USERS
// ============================================================================
constexpr const char* MSG_USER_SELECT                   = QT_TRANSLATE_NOOP("Messages", "Please select a user first.");
constexpr const char* MSG_USER_ADD_SUCCESS              = QT_TRANSLATE_NOOP("Messages", "User created successfully.");
constexpr const char* MSG_USER_EDIT_SUCCESS             = QT_TRANSLATE_NOOP("Messages", "User updated successfully.");
constexpr const char* MSG_USER_DELETE_CONFIRM           = QT_TRANSLATE_NOOP("Messages", "Are you sure you want to delete this user?");
constexpr const char* MSG_USER_DELETE_SUCCESS           = QT_TRANSLATE_NOOP("Messages", "User deleted successfully.");
constexpr const char* MSG_USER_CANNOT_DELETE_SELF       = QT_TRANSLATE_NOOP("Messages", "You cannot delete your own account.");
constexpr const char* MSG_USER_CANNOT_DELETE_LAST_ADMIN = QT_TRANSLATE_NOOP("Messages", "Cannot delete the last Admin account.");

// ============================================================================
// MESSAGES - DATABASE
// ============================================================================

// Backup filename written to the sync folder

// Files that must exist inside a valid POSBox backup zip

// ============================================================================
// MESSAGES - PRINTER
// ============================================================================
constexpr const char* MSG_PRINTER_SAVE_SUCCESS   = QT_TRANSLATE_NOOP("Messages", "Printer settings saved successfully.");
constexpr const char* MSG_PRINTER_SAVE_FAILED    = QT_TRANSLATE_NOOP("Messages", "Failed to save printer settings.");
constexpr const char* MSG_PRINTER_TEST_SENT      = QT_TRANSLATE_NOOP("Messages", "Test print sent.");
constexpr const char* MSG_PRINTER_TEST_FAILED    = QT_TRANSLATE_NOOP("Messages", "Failed to send test print. Check port and connection.");
constexpr const char* MSG_PRINTER_NO_PORT        = QT_TRANSLATE_NOOP("Messages", "Please select a COM port first.");
constexpr const char* MSG_PRINTER_NO_PORTS_FOUND = QT_TRANSLATE_NOOP("Messages", "No serial ports detected.");
constexpr const char* MSG_PRINTER_DISABLED       = QT_TRANSLATE_NOOP("Messages", "Thermal printer is disabled. Enable it above to configure.");

// ============================================================================
// BACKUP & RESTORE (local, manual)
// ============================================================================
constexpr const char* MSG_BACKUP_SUCCESS        = QT_TRANSLATE_NOOP("Messages", "Backup created successfully.");
constexpr const char* MSG_BACKUP_PATH_MISSING   = QT_TRANSLATE_NOOP("Messages", "Please set a backup folder path first.");
constexpr const char* MSG_BACKUP_ZIP_FAILED     = QT_TRANSLATE_NOOP("Messages", "Failed to create zip archive. Make sure %1 is installed.");
constexpr const char* MSG_IMPORT_INVALID_ZIP    = QT_TRANSLATE_NOOP("Messages", "The selected file does not appear to be a valid POSBox backup.\n"
                                                  "Please select a .zip file created by this software.");
constexpr const char* MSG_IMPORT_CONFIRM        = QT_TRANSLATE_NOOP("Messages", "Replace all current data with this backup? This cannot be undone.");
constexpr const char* MSG_IMPORT_FAILED         = QT_TRANSLATE_NOOP("Messages", "Import failed. The backup file may be corrupt or incomplete.");
constexpr const char* MSG_IMPORT_SUCCESS        = QT_TRANSLATE_NOOP("Messages", "Import successful.\n\nAll POSBox apps will now close.\n"
                                                  "Open POSBox again to use the restored data.");
constexpr const char* BACKUP_FILENAME           = "POSBox_backup.zip";
// A POSBox backup always holds the database file.
constexpr const char* BACKUP_MARKER_DB          = "pos_database.db";

} // namespace SettingsConstants

#endif // SETTINGSCONSTANTS_H
