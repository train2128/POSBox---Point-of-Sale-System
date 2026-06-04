#include "SessionManager.h"
#include <QDebug>

SessionManager& SessionManager::instance()
{
    static SessionManager instance;
    return instance;
}

SessionManager::SessionManager(QObject* parent)
    : QObject(parent)
    , m_nextSessionId(1)
{
    qDebug() << "SessionManager initialized";
}

SessionManager::~SessionManager()
{
    // Clean up all sessions
    qDeleteAll(m_sessions);
    m_sessions.clear();
}

int SessionManager::createSession(const QString& username, const QString& role, const QString& displayName)
{
    if (username.isEmpty() || role.isEmpty()) {
        qWarning() << "Cannot create session with empty username or role";
        return -1;
    }

    int sessionId = m_nextSessionId++;

    UserSession* session = new UserSession(sessionId, username, role, displayName, this);
    m_sessions.insert(sessionId, session);

    qDebug() << "Session created:" << sessionId << "-" << username << "(" << role << ")";

    emit sessionCreated(sessionId);

    return sessionId;
}

UserSession* SessionManager::getSession(int sessionId)
{
    return m_sessions.value(sessionId, nullptr);
}

void SessionManager::closeSession(int sessionId)
{
    if (!m_sessions.contains(sessionId)) {
        qWarning() << "Cannot close non-existent session:" << sessionId;
        return;
    }

    UserSession* session = m_sessions.take(sessionId);

    qDebug() << "Session closed:" << sessionId << "-" << session->username();

    emit sessionClosed(sessionId);

    delete session;
}

QList<int> SessionManager::activeSessions() const
{
    return m_sessions.keys();
}
