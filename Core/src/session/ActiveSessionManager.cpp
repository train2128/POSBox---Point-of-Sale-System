#include "ActiveSessionManager.h"
#include "CommonConfig.h"
#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>

ActiveSessionManager::ActiveSessionManager()
    : m_sessionFilePath(POSConfig::getAppDataPath() + "active_sessions.json")
    , m_lockFile(POSConfig::getAppDataPath() + "active_sessions.lock")
{
}

ActiveSessionManager& ActiveSessionManager::instance()
{
    static ActiveSessionManager instance;
    return instance;
}

ActiveSessionManager::SessionInfo ActiveSessionManager::SessionInfo::fromJson(const QJsonObject& obj)
{
    SessionInfo info;
    info.username = obj.value("username").toString();
    info.role = obj.value("role").toString();
    info.displayName = obj.value("displayName").toString();
    info.appName = obj.value("appName").toString();
    info.timestamp = obj.value("timestamp").toString();
    return info;
}

QJsonObject ActiveSessionManager::SessionInfo::toJson() const
{
    return QJsonObject{
        {"username", username},
        {"role", role},
        {"displayName", displayName},
        {"appName", appName},
        {"timestamp", timestamp},
    };
}

ActiveSessionManager::SessionInfo ActiveSessionManager::currentSession() const
{
    QFile file(m_sessionFilePath);
    if (!file.open(QIODevice::ReadOnly))
        return SessionInfo();

    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << "ActiveSessionManager: invalid session file:" << error.errorString();
        return SessionInfo();
    }

    const QJsonArray sessions = doc.object().value("sessions").toArray();
    return sessions.isEmpty() ? SessionInfo() : SessionInfo::fromJson(sessions.first().toObject());
}

bool ActiveSessionManager::setCurrentSession(const SessionInfo& info)
{
    if (!m_lockFile.tryLock(5000)) {
        qWarning() << "ActiveSessionManager: Failed to acquire lock";
        return false;
    }

    bool written = false;
    QSaveFile file(m_sessionFilePath);
    if (file.open(QIODevice::WriteOnly)) {
        const QJsonObject root{{"sessions", QJsonArray{info.toJson()}}};
        file.write(QJsonDocument(root).toJson());
        written = file.commit();
    }
    if (!written)
        qWarning() << "ActiveSessionManager: Failed to write session file";

    m_lockFile.unlock();
    return written;
}

void ActiveSessionManager::clearCurrentSession()
{
    if (!m_lockFile.tryLock(5000)) {
        qWarning() << "ActiveSessionManager: Failed to acquire lock";
        return;
    }
    QFile::remove(m_sessionFilePath);
    m_lockFile.unlock();
}
