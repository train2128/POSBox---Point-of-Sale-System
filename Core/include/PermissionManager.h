#ifndef PERMISSIONMANAGER_H
#define PERMISSIONMANAGER_H

#include <QString>
#include <QSqlDatabase>
#include <QMap>

class PermissionManager
{
public:
    explicit PermissionManager(QSqlDatabase db = QSqlDatabase());
    ~PermissionManager();

    QMap<QString, bool> loadUserPermissions(const QString& username);
    void saveUserPermission(const QString& username, const QString& permissionKey, bool allowed);
    void clearUserPermission(const QString& username, const QString& permissionKey);
    void clearAllUserPermissions(const QString& username);

    bool hasPermissionOverride(const QString& username, const QString& permissionKey);

    QStringList getAvailablePermissions() const;
    QString getPermissionDisplayName(const QString& permissionKey) const;
    QString getPermissionGroup(const QString& permissionKey) const;

private:
    QSqlDatabase m_db;
};

#endif // PERMISSIONMANAGER_H
