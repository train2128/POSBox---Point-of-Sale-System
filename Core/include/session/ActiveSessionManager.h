#ifndef ACTIVESESSIONMANAGER_H
#define ACTIVESESSIONMANAGER_H

#include <QString>
#include <QJsonObject>
#include <QLockFile>

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

    // Single-session model: only one user session may be active at a time.
    // The Launcher is the sole writer; apps only read.
    SessionInfo currentSession() const;
    bool setCurrentSession(const SessionInfo& info);
    void clearCurrentSession();

private:
    ActiveSessionManager();

    ActiveSessionManager(const ActiveSessionManager&) = delete;
    ActiveSessionManager& operator=(const ActiveSessionManager&) = delete;

    QString m_sessionFilePath;
    mutable QLockFile m_lockFile;
};

#endif // ACTIVESESSIONMANAGER_H
