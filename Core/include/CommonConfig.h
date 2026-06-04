#ifndef COMMONCONFIG_H
#define COMMONCONFIG_H

#include <QString>

namespace POSConfig {

// Organization & Application
constexpr const char* ORG_NAME     = "TecBox";
constexpr const char* APP_NAME     = "POSBox";
constexpr const char* APP_VERSION  = "2.0.3-c";

// Database
constexpr const char* DB_FILENAME  = "pos_database.db";
constexpr const char* DB_FOLDER    = "POSBox";

// Shared asset folders inside getAppDataPath()
// Used by any app that needs to resolve stored filenames to full paths.
constexpr const char* LOGO_IMAGE_FOLDER     = "logo";
constexpr const char* PRODUCT_IMAGE_FOLDER  = "products_images";
constexpr const char* DOCUMENTS_FOLDER      = "Documents";

// Cross-platform path resolver
QString getDatabasePath();

// App data directory
QString getAppDataPath();

// ── App settings file (shared between StyleManager and SettingsWindow) ──────
constexpr const char* SETTINGS_FILENAME      = "settings.json";
constexpr const char* COLOR_SCHEME_DARK      = "dark";
constexpr const char* COLOR_SCHEME_LIGHT     = "light";
constexpr const char* FONT_SIZE_SMALL        = "small";
constexpr const char* FONT_SIZE_MEDIUM       = "medium";
constexpr const char* FONT_SIZE_LARGE        = "large";

// Background identifiers (saved to settings.json as "background" key)
constexpr const char* BG_DEFAULT  = "default";
constexpr const char* BG_CARAMEL  = "caramel";
constexpr const char* BG_BLACK    = "black";
constexpr const char* BG_BLUE     = "blue";

// Background resource paths
constexpr const char* BG_PATH_DEFAULT  = ":/Backgrounds/images/Backgrounds/main_bg.png";
constexpr const char* BG_PATH_CARAMEL  = ":/Backgrounds/images/Backgrounds/main_bg_caramel.png";
constexpr const char* BG_PATH_BLACK    = ":/Backgrounds/images/Backgrounds/main_bg_black.png";
constexpr const char* BG_PATH_BLUE     = ":/Backgrounds/images/Backgrounds/main_bg_blue.png";

} // namespace POSConfig

#endif // COMMONCONFIG_H
