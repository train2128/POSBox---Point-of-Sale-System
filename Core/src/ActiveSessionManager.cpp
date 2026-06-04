#include "ActiveSessionManager.h"
#include "CommonConfig.h"
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDebug>
#include <QLockFile>
#include <QFileInfo>
#include <QSaveFile>

ActiveSessionManager::ActiveSessionManager()
    : m_lockValid(false)
{
    const QString appDataPath = POSConfig::getAppDataPath();
    m_sessionFilePath = appDataPath + "active_sessions.json";
    m_lockFilePath = appDataPath + "active_sessions.lock";
}

ActiveSessionManager::~ActiveSessionManager()
{
    if (m_lockValid) {
        releaseLock();
    }
}

ActiveSessionManager& ActiveSessionManager::instance()
{
    static ActiveSessionManager instance;
    return instance;
}

QString ActiveSessionManager::getSessionFilePath() const
{
    return m_sessionFilePath;
}

QString ActiveSessionManager::getLockFilePath() const
{
    return m_lockFilePath;
}

bool ActiveSessionManager::acquireLock()
{
    QLockFile lockFile(m_lockFilePath);
    
    if (!lockFile.tryLock(5000)) {
        qWarning() << "ActiveSessionManager: Failed to acquire lock";
        return false;
    }
    
    m_lockValid = true;
    return true;
}

void ActiveSessionManager::releaseLock()
{
    if (m_lockValid) {
        QFile::remove(m_lockFilePath);
        m_lockValid = false;
    }
}

bool ActiveSessionManager::readSessions()
{
    m_root = QJsonObject();
    
    QFile file(m_sessionFilePath);
    if (!file.exists()) {
        return true;
    }
    
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "ActiveSessionManager: Failed to open session file for reading";
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    
    if (error.error != QJsonParseError::NoError) {
        qWarning() << "ActiveSessionManager: JSON parse error:" << error.errorString();
        return false;
    }
    
    if (!doc.isObject()) {
        qWarning() << "ActiveSessionManager: Session file is not an object";
        return false;
    }
    
    m_root = doc.object();
    return true;
}

bool ActiveSessionManager::writeSessions()
{
    QDir().mkpath(QFileInfo(m_sessionFilePath).absolutePath());
    
    QSaveFile file(m_sessionFilePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "ActiveSessionManager: Failed to open session file for writing";
        return false;
    }
    
    QJsonDocument doc(m_root);
    file.write(doc.toJson());
    
    if (!file.commit()) {
        qWarning() << "ActiveSessionManager: Failed to commit session file";
        return false;
    }
    
    return true;
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
    QJsonObject obj;
    obj["username"] = username;
    obj["role"] = role;
    obj["displayName"] = displayName;
    obj["appName"] = appName;
    obj["timestamp"] = timestamp;
    return obj;
}

bool ActiveSessionManager::hasSession(const QString& username) const
{
    const_cast<ActiveSessionManager*>(this)->readSessions();
    
    QJsonArray sessions = m_root.value("sessions").toArray();
    for (const auto& val : sessions) {
        if (val.toObject().value("username").toString() == username) {
            return true;
        }
    }
    return false;
}

ActiveSessionManager::SessionInfo ActiveSessionManager::getSession(const QString& username) const
{
    const_cast<ActiveSessionManager*>(this)->readSessions();
    
    QJsonArray sessions = m_root.value("sessions").toArray();
    for (const auto& val : sessions) {
        QJsonObject obj = val.toObject();
        if (obj.value("username").toString() == username) {
            return SessionInfo::fromJson(obj);
        }
    }
    return SessionInfo();
}

bool ActiveSessionManager::addSession(const SessionInfo& info)
{
    if (!acquireLock()) {
        return false;
    }
    
    if (!readSessions()) {
        releaseLock();
        return false;
    }
    
    QJsonArray sessions = m_root.value("sessions").toArray();
    
    for (int i = 0; i < sessions.size(); ++i) {
        if (sessions[i].toObject().value("username").toString() == info.username) {
            sessions[i] = info.toJson();
            m_root["sessions"] = sessions;
            bool result = writeSessions();
            releaseLock();
            return result;
        }
    }
    
    sessions.append(info.toJson());
    m_root["sessions"] = sessions;
    
    bool result = writeSessions();
    releaseLock();
    return result;
}

bool ActiveSessionManager::removeSession(const QString& username)
{
    if (!acquireLock()) {
        return false;
    }
    
    if (!readSessions()) {
        releaseLock();
        return false;
    }
    
    QJsonArray sessions = m_root.value("sessions").toArray();
    QJsonArray newSessions;
    
    for (const auto& val : sessions) {
        QJsonObject obj = val.toObject();
        if (obj.value("username").toString() != username) {
            newSessions.append(obj);
        }
    }
    
    m_root["sessions"] = newSessions;
    
    if (newSessions.isEmpty()) {
        QFile::remove(m_sessionFilePath);
        releaseLock();
        return true;
    }
    
    bool result = writeSessions();
    releaseLock();
    return result;
}

bool ActiveSessionManager::updateSession(const QString& username, const SessionInfo& info)
{
    return addSession(info);
}

void ActiveSessionManager::cleanupOnExit(const QString& username)
{
    removeSession(username);
    qDebug() << "ActiveSessionManager: Cleaned up session for" << username;
}

void ActiveSessionManager::cleanupOrphanedSessions()
{
    if (!acquireLock()) {
        return;
    }
    
    if (!readSessions()) {
        releaseLock();
        return;
    }
    
    QJsonArray sessions = m_root.value("sessions").toArray();
    if (sessions.isEmpty()) {
        releaseLock();
        return;
    }
    
    QJsonArray newSessions;
    QDateTime now = QDateTime::currentDateTime();
    
    for (const auto& val : sessions) {
        QJsonObject obj = val.toObject();
        QString timestampStr = obj.value("timestamp").toString();
        QDateTime sessionTime = QDateTime::fromString(timestampStr, Qt::ISODate);
        
        if (sessionTime.isValid()) {
            newSessions.append(obj);
        }
    }
    
    if (newSessions.isEmpty()) {
        QFile::remove(m_sessionFilePath);
    } else {
        m_root["sessions"] = newSessions;
        writeSessions();
    }
    
    releaseLock();
    qDebug() << "ActiveSessionManager: Cleanup completed";
}
