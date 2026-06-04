#ifndef ACTIVESESSIONMANAGER_H
#define ACTIVESESSIONMANAGER_H

#include <QString>
#include <QDateTime>
#include <QJsonObject>

class ActiveSessionManager
{
public:
    struct SessionInfo {
        QString username;
        QString role;
        QString displayName;
        QString appName;
        QString timestamp;
        
        QJsonObject toJson() const;
        static SessionInfo fromJson(const QJsonObject& obj);
    };

    static ActiveSessionManager& instance();

    bool hasSession(const QString& username) const;
    SessionInfo getSession(const QString& username) const;
    bool addSession(const SessionInfo& info);
    bool removeSession(const QString& username);
    bool updateSession(const QString& username, const SessionInfo& info);
    
    void cleanupOrphanedSessions();
    void cleanupOnExit(const QString& username);

private:
    ActiveSessionManager();
    ~ActiveSessionManager();
    
    ActiveSessionManager(const ActiveSessionManager&) = delete;
    ActiveSessionManager& operator=(const ActiveSessionManager&) = delete;
    
    bool acquireLock();
    void releaseLock();
    bool readSessions();
    bool writeSessions();
    QString getSessionFilePath() const;
    QString getLockFilePath() const;
    
    QString m_sessionFilePath;
    QString m_lockFilePath;
    QJsonObject m_root;
    bool m_lockValid;
};

#endif // ACTIVESESSIONMANAGER_H
