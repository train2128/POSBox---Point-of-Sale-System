#include "PermissionManager.h"
#include "AccessControl.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

struct PermissionInfo {
    QString displayName;
    QString group;
};

static const QMap<QString, PermissionInfo>& getPermissionMap()
{
    static const QMap<QString, PermissionInfo> permissions = {
        { AccessControl::PERMISSION_WITHDRAW_CASH,    { "Withdraw Cash",       "Register" } },
        { AccessControl::PERMISSION_DELETE_TICKET,     { "Delete Ticket",       "Tickets" } },
        { AccessControl::PERMISSION_DELETE_PRODUCT,    { "Delete Products",     "Inventory" } },
        { AccessControl::PERMISSION_ADD_CATEGORY,     { "Add Categories",      "Inventory" } },
        { AccessControl::PERMISSION_MANAGE_DISCOUNTS, { "Manage Discounts",    "Inventory" } }
    };
    return permissions;
}

PermissionManager::PermissionManager(QSqlDatabase db)
    : m_db(db)
{
}

PermissionManager::~PermissionManager()
{
}

QMap<QString, bool> PermissionManager::loadUserPermissions(const QString& username)
{
    QMap<QString, bool> permissions;

    if (username.isEmpty()) {
        return permissions;
    }

    QSqlQuery query(m_db);
    query.prepare("SELECT permission_key, is_allowed FROM user_permissions WHERE username = ?");
    query.addBindValue(username);

    if (!query.exec()) {
        qWarning() << "Failed to load user permissions:" << query.lastError().text();
        return permissions;
    }

    while (query.next()) {
        QString key = query.value(0).toString();
        bool allowed = query.value(1).toInt() == 1;
        permissions[key] = allowed;
    }

    return permissions;
}

void PermissionManager::saveUserPermission(const QString& username, const QString& permissionKey, bool allowed)
{
    if (username.isEmpty() || permissionKey.isEmpty()) {
        return;
    }

    QSqlQuery query(m_db);
    query.prepare(R"(
        INSERT INTO user_permissions (username, permission_key, is_allowed)
        VALUES (?, ?, ?)
        ON CONFLICT(username, permission_key) DO UPDATE SET is_allowed = ?
    )");
    query.addBindValue(username);
    query.addBindValue(permissionKey);
    query.addBindValue(allowed ? 1 : 0);
    query.addBindValue(allowed ? 1 : 0);

    if (!query.exec()) {
        qWarning() << "Failed to save user permission:" << query.lastError().text();
    }
}

void PermissionManager::clearUserPermission(const QString& username, const QString& permissionKey)
{
    if (username.isEmpty() || permissionKey.isEmpty()) {
        return;
    }

    QSqlQuery query(m_db);
    query.prepare("DELETE FROM user_permissions WHERE username = ? AND permission_key = ?");
    query.addBindValue(username);
    query.addBindValue(permissionKey);

    if (!query.exec()) {
        qWarning() << "Failed to clear user permission:" << query.lastError().text();
    }
}

void PermissionManager::clearAllUserPermissions(const QString& username)
{
    if (username.isEmpty()) {
        return;
    }

    QSqlQuery query(m_db);
    query.prepare("DELETE FROM user_permissions WHERE username = ?");
    query.addBindValue(username);

    if (!query.exec()) {
        qWarning() << "Failed to clear all user permissions:" << query.lastError().text();
    }
}

bool PermissionManager::hasPermissionOverride(const QString& username, const QString& permissionKey)
{
    if (username.isEmpty() || permissionKey.isEmpty()) {
        return false;
    }

    QSqlQuery query(m_db);
    query.prepare("SELECT COUNT(*) FROM user_permissions WHERE username = ? AND permission_key = ?");
    query.addBindValue(username);
    query.addBindValue(permissionKey);

    if (query.exec() && query.next()) {
        return query.value(0).toInt() > 0;
    }

    return false;
}

QStringList PermissionManager::getAvailablePermissions() const
{
    return getPermissionMap().keys();
}

QString PermissionManager::getPermissionDisplayName(const QString& permissionKey) const
{
    return getPermissionMap().value(permissionKey).displayName;
}

QString PermissionManager::getPermissionGroup(const QString& permissionKey) const
{
    return getPermissionMap().value(permissionKey).group;
}
