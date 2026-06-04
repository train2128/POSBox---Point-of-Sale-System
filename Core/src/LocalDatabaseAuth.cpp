#include "LocalDatabaseAuth.h"
#include "DatabaseManager.h"
#include "QueryRepository.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

LocalDatabaseAuth::LocalDatabaseAuth(DatabaseManager* dbManager)
    : m_dbManager(dbManager)
{
}

AuthResult LocalDatabaseAuth::authenticate(const QString& username, const QString& password)
{
    if (!m_dbManager || !m_dbManager->isOpen()) {
        return AuthResult::Failure("Database connection not available");
    }

    QSqlQuery query(m_dbManager->database());
    query.prepare(Queries::ACCOUNT_SELECT_BY_USERNAME);
    query.addBindValue(username);

    if (!query.exec()) {
        qWarning() << "Authentication query failed:" << query.lastError().text();
        return AuthResult::Failure("Database query failed: " + query.lastError().text());
    }

    if (!query.next()) {
        qDebug() << "Authentication failed: User not found:" << username;
        return AuthResult::Failure("Invalid username or password");
    }

    // Get user data
    QString storedPassword = query.value("password_hash").toString();
    QString role = query.value("role").toString();
    QString displayName = query.value("display_name").toString();
    bool isActive = query.value("is_active").toBool();

    // Check if account is active
    if (!isActive) {
        qDebug() << "Authentication failed: Account disabled:" << username;
        return AuthResult::Failure("This account has been disabled");
    }

    // Verify password (plain text for now - TODO: implement hashing)
    if (storedPassword != password) {
        qDebug() << "Authentication failed: Invalid password for user:" << username;
        return AuthResult::Failure("Invalid username or password");
    }

    // Update last login timestamp
    QSqlQuery updateQuery(m_dbManager->database());
    updateQuery.prepare(Queries::ACCOUNT_UPDATE_LAST_LOGIN);
    updateQuery.addBindValue(username);
    if (!updateQuery.exec()) {
        qWarning() << "Failed to update last login:" << updateQuery.lastError().text();
        // Don't fail authentication if this fails
    }

    qDebug() << "Authentication successful:" << username << "(" << role << ")";

    // Use display name if available, otherwise use username
    QString finalDisplayName = displayName.isEmpty() ? username : displayName;

    return AuthResult::Success(username, role, finalDisplayName);
}

bool LocalDatabaseAuth::isAvailable() const
{
    return m_dbManager && m_dbManager->isOpen();
}
