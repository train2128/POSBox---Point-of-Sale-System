#include "CommonConfig.h"
#include <QStandardPaths>
#include <QDir>
#include <QDebug>

namespace POSConfig {

QString getAppDataPath()
{
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

#ifdef Q_OS_WIN
    // Windows: Use Roaming explicitly
    appDataPath = QDir::homePath() + "/AppData/Roaming";
#endif

    QString posboxPath = appDataPath + "/" + DB_FOLDER;

    QDir dir;
    if (!dir.exists(posboxPath)) {
        if (!dir.mkpath(posboxPath)) {
            qCritical() << "Failed to create POSBox directory:" << posboxPath;
            return QString();
        }
    }

    return posboxPath + "/";
}

QString getDatabasePath()
{
    QString appDataPath = getAppDataPath();
    if (appDataPath.isEmpty()) {
        return QString();
    }

    return appDataPath + "/" + DB_FILENAME;
}

} // namespace POSConfig
