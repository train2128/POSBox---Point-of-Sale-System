#ifndef USERSESSION_H
#define USERSESSION_H

#include <QObject>
#include <QString>

/**
 * Represents a single user session
 * NOT a singleton - multiple sessions can exist simultaneously
 */
class UserSession : public QObject
{
    Q_OBJECT

public:
    explicit UserSession(int sessionId, const QString& username, const QString& role,
                         const QString& displayName, QObject* parent = nullptr);

    // Getters
    int sessionId() const { return m_sessionId; }
    QString username() const { return m_username; }
    QString role() const { return m_role; }
    QString displayName() const { return m_displayName; }

    // Session info
    QString sessionInfo() const;

private:
    int m_sessionId;
    QString m_username;
    QString m_role;
    QString m_displayName;
};

#endif // USERSESSION_H
