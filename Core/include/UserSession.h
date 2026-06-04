#ifndef USERSESSION_H
#define USERSESSION_H

#include <QObject>
#include <QString>
#include <QDateTime>

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
    ~UserSession();

    // Getters
    int sessionId() const { return m_sessionId; }
    QString username() const { return m_username; }
    QString role() const { return m_role; }
    QString displayName() const { return m_displayName; }
    QDateTime loginTime() const { return m_loginTime; }

    // Role checks
    bool isAdmin() const { return m_role == "Admin"; }
    bool isManager() const { return m_role == "Manager"; }
    bool isCashier() const { return m_role == "Cashier" || m_role == "User"; }

    // Session info
    QString sessionInfo() const;

signals:
    void sessionClosing(int sessionId);

private:
    int m_sessionId;
    QString m_username;
    QString m_role;
    QString m_displayName;
    QDateTime m_loginTime;
};

#endif // USERSESSION_H
