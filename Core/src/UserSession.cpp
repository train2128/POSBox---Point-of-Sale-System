#include "UserSession.h"

UserSession::UserSession(int sessionId, const QString& username, const QString& role,
                         const QString& displayName, QObject* parent)
    : QObject(parent)
    , m_sessionId(sessionId)
    , m_username(username)
    , m_role(role)
    , m_displayName(displayName)
    , m_loginTime(QDateTime::currentDateTime())
{
}

UserSession::~UserSession()
{
    emit sessionClosing(m_sessionId);
}

QString UserSession::sessionInfo() const
{
    return QString("%1 (%2) - Session #%3")
    .arg(m_displayName)
        .arg(m_role)
        .arg(m_sessionId);
}
