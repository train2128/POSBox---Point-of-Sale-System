#include "CommonConfig.h"
#include <QDebug>
#include <QDir>
#include <QStandardPaths>

namespace POSConfig {

QString getAppDataPath()
{
    // Called many times per run: resolve and create the folder only once.
    static const QString path = []() -> QString {
#ifdef Q_OS_WIN
        const QString base = QDir::homePath() + "/AppData/Roaming";
#else
        const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
#endif
        const QString dir = base + "/" + DATA_FOLDER;
        if (!QDir().mkpath(dir)) {
            qCritical() << "Failed to create POSBox directory:" << dir;
            return {};
        }
        return dir + "/";
    }();
    return path;
}

} // namespace POSConfig

namespace POSConfig {

QString getDatabasePath()
{
    const QString dir = getAppDataPath();
    return dir.isEmpty() ? QString() : dir + DB_FILENAME;
}

} // namespace POSConfig
