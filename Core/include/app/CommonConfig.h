#ifndef COMMONCONFIG_H
#define COMMONCONFIG_H

#include <QString>

namespace POSConfig {

// Organization & application identity
constexpr const char* ORG_NAME     = "TecBox";
constexpr const char* APP_NAME     = "POSBox";
constexpr const char* APP_VERSION  = "2.1.5-c";

// Folder for per-machine files, inside the system app-data location
constexpr const char* DATA_FOLDER  = "POSBox";

// The local SQLite database (Commercial Version: everything lives in it)
constexpr const char* DB_FILENAME  = "pos_database.db";

// Image folders inside getAppDataPath(). The database stores file names only.
constexpr const char* LOGO_IMAGE_FOLDER    = "logo";
constexpr const char* PRODUCT_IMAGE_FOLDER = "products_images";

// Absolute per-machine data directory, with a trailing '/'.
// Requires the organization and application names to be set (AppRuntime does).
QString getAppDataPath();

// Absolute path of the local database file.
QString getDatabasePath();

// Theme value stored in the settings
constexpr const char* COLOR_SCHEME_LIGHT = "light";

} // namespace POSConfig

#endif // COMMONCONFIG_H
