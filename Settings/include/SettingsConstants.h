#ifndef SETTINGSCONSTANTS_H
#define SETTINGSCONSTANTS_H

#include "PrinterConstants.h"

/**
 * Settings-specific constants.
 * Printer JSON constants live in Core/PrinterConstants.h (shared with ThermalPrinter).
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
constexpr const char* PHONE_PREFIX       = "+216";
constexpr int         PHONE_DIGIT_COUNT  = 8;
constexpr int         POSTAL_DIGIT_COUNT = 4;

// ============================================================================
// MESSAGES - GENERAL
// ============================================================================
constexpr const char* MSG_ACCESS_DENIED  = "Access denied. Only Admins can access Settings.";
constexpr const char* MSG_SAVE_SUCCESS   = "Shop profile saved successfully.";
constexpr const char* MSG_SAVE_FAILED    = "Failed to save shop profile.";
constexpr const char* MSG_CANCEL_CONFIRM = "Discard unsaved changes?";

// ============================================================================
// MESSAGES - VALIDATION
// ============================================================================
constexpr const char* MSG_SHOP_NAME_EMPTY = "Shop name cannot be empty.";
constexpr const char* MSG_PHONE_INVALID   = "Phone number must contain exactly %1 digits after the prefix.";
constexpr const char* MSG_POSTAL_INVALID  = "Postal code must contain exactly %1 digits.";
constexpr const char* MSG_EMAIL_INVALID   = "Please enter a valid email address.";

// ============================================================================
// MESSAGES - USERS
// ============================================================================
constexpr const char* MSG_USER_SELECT                   = "Please select a user first.";
constexpr const char* MSG_USER_ADD_SUCCESS              = "User created successfully.";
constexpr const char* MSG_USER_EDIT_SUCCESS             = "User updated successfully.";
constexpr const char* MSG_USER_DELETE_CONFIRM           = "Are you sure you want to delete this user?";
constexpr const char* MSG_USER_DELETE_SUCCESS           = "User deleted successfully.";
constexpr const char* MSG_USER_CANNOT_DELETE_SELF       = "You cannot delete your own account.";
constexpr const char* MSG_USER_CANNOT_DELETE_LAST_ADMIN = "Cannot delete the last Admin account.";

// ============================================================================
// MESSAGES - DATABASE
// ============================================================================
constexpr const char* MSG_SYNC_PATH_EMPTY       = "Please set a backup folder path first.";
constexpr const char* MSG_SYNC_PATH_SAVED       = "Backup folder path saved.";

constexpr const char* MSG_BACKUP_SUCCESS        = "Backup created successfully.";
constexpr const char* MSG_BACKUP_FAILED         = "Backup failed. Check the folder path and try again.";
constexpr const char* MSG_BACKUP_PATH_MISSING   = "Please set a backup folder path first.";
constexpr const char* MSG_BACKUP_ZIP_FAILED     = "Failed to create zip archive. Make sure %1 is installed.";

constexpr const char* MSG_IMPORT_INVALID_ZIP    = "The selected file does not appear to be a valid POSBox backup.\n"
                                               "Please select a .zip file created by this software.";
constexpr const char* MSG_IMPORT_CONFIRM        = "This will replace all current data with the contents of the backup.\n"
                                           "This action cannot be undone. Continue?";
constexpr const char* MSG_IMPORT_FAILED         = "Import failed. The backup file may be corrupt or incomplete.";
constexpr const char* MSG_IMPORT_SUCCESS        = "Import successful.\n\nThe application will now close.\n"
                                           "Please restart it to apply the imported data.";

// Backup filename written to the sync folder
constexpr const char* BACKUP_FILENAME           = "POSBox_backup.zip";

// Files that must exist inside a valid POSBox backup zip
constexpr const char* BACKUP_MARKER_DB          = "pos_database.db";
constexpr const char* BACKUP_MARKER_SETTINGS    = "settings.json";

// ============================================================================
// MESSAGES - PRINTER
// ============================================================================
constexpr const char* MSG_PRINTER_SAVE_SUCCESS   = "Printer settings saved successfully.";
constexpr const char* MSG_PRINTER_SAVE_FAILED    = "Failed to save printer settings.";
constexpr const char* MSG_PRINTER_TEST_SENT      = "Test print sent.";
constexpr const char* MSG_PRINTER_TEST_FAILED    = "Failed to send test print. Check port and connection.";
constexpr const char* MSG_PRINTER_NO_PORT        = "Please select a COM port first.";
constexpr const char* MSG_PRINTER_NO_PORTS_FOUND = "No serial ports detected.";
constexpr const char* MSG_PRINTER_DISABLED       = "Thermal printer is disabled. Enable it above to configure.";

// ============================================================================
// SETTINGS JSON KEYS
// (used by Settings, Register, Financial, Tickets to read app-wide preferences)
// Note: SETTINGS_FILENAME, COLOR_SCHEME_*, FONT_SIZE_* are defined
// in CommonConfig.h (POSConfig namespace) so Core's StyleManager can access them.
// ============================================================================

// Whether out-of-stock products are shown in the Register product grid
// Default: true (show OOS items). When false, OOS stocked products are hidden.
// Services and variant products are never affected by this setting.
constexpr const char* KEY_SHOW_OOS        = "show_oos_products";

} // namespace SettingsConstants

#endif // SETTINGSCONSTANTS_H
