#ifndef SESSIONMANAGER_H
#define SESSIONMANAGER_H

#include <QObject>
#include <QHash>
#include <QList>
#include "UserSession.h"

/**
 * Central manager for all user sessions
 * Allows multiple users to be logged in simultaneously across different windows
 */
class SessionManager : public QObject
{
    Q_OBJECT

public:
    static SessionManager& instance();

    /**
     * Create a new session after successful login
     * @return Session ID (> 0) or -1 on failure
     */
    int createSession(const QString& username, const QString& role, const QString& displayName);

    /**
     * Get session by ID
     * @return UserSession pointer or nullptr if not found
     */
    UserSession* getSession(int sessionId);

    /**
     * Close a session (logout)
     */
    void closeSession(int sessionId);

    /**
     * Get list of all active session IDs
     */
    QList<int> activeSessions() const;

    /**
     * Get number of active sessions
     */
    int sessionCount() const { return m_sessions.size(); }

    /**
     * Check if a session exists
     */
    bool hasSession(int sessionId) const { return m_sessions.contains(sessionId); }

signals:
    void sessionCreated(int sessionId);
    void sessionClosed(int sessionId);

private:
    SessionManager(QObject* parent = nullptr);
    ~SessionManager();

    // Non-copyable
    SessionManager(const SessionManager&) = delete;
    SessionManager& operator=(const SessionManager&) = delete;

    QHash<int, UserSession*> m_sessions;
    int m_nextSessionId;
};

#endif // SESSIONMANAGER_H
