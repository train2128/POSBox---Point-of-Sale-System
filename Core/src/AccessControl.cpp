#include "AccessControl.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

bool AccessControl::canAccessPage(Page page, Role role)
{
    if (role == Admin) {
        return true;
    }

    switch (page) {
    case RegisterPage:
        return true;

    case TicketsPage:
        return true;

    case InventoryPage:
        return (role == Manager);

    case FinancesPage:
        return (role == Manager);

    case SettingsPage:
        return false;

    default:
        return false;
    }
}

bool AccessControl::canAccessPage(const QString& username, Page page, QSqlDatabase db)
{
    if (username.isEmpty()) {
        return false;
    }

    QSqlQuery query(db);
    query.prepare("SELECT role FROM accounts WHERE username = ?");
    query.addBindValue(username);

    if (!query.exec() || !query.next()) {
        return false;
    }

    QString roleStr = query.value(0).toString();
    return canAccessPage(page, stringToRole(roleStr));
}

bool AccessControl::canPerformAction(Action action, Role role)
{
    if (role == Admin) {
        return true;
    }

    return getRoleDefaultPermission(action, role);
}

bool AccessControl::canPerformAction(const QString& username, Action action, QSqlDatabase db)
{
    QString permKey = actionToPermissionKey(action);
    return canPerformAction(username, permKey, db);
}

bool AccessControl::canPerformAction(const QString& username, const QString& permissionKey, QSqlDatabase db)
{
    if (username.isEmpty()) {
        return false;
    }

    QSqlQuery query(db);
    query.prepare("SELECT role FROM accounts WHERE username = ?");
    query.addBindValue(username);

    if (!query.exec() || !query.next()) {
        return false;
    }

    QString roleStr = query.value(0).toString();
    Role role = stringToRole(roleStr);

    if (role == Admin) {
        return true;
    }

    int override = getUserPermissionOverride(username, permissionKey, db);
    if (override == 1) {
        return true;
    }
    if (override == 0) {
        return false;
    }

    Action action = permissionKeyToAction(permissionKey);
    return getRoleDefaultPermission(action, role);
}

bool AccessControl::getUserPermissionOverride(const QString& username, const QString& permissionKey, QSqlDatabase db)
{
    QSqlQuery query(db);
    query.prepare("SELECT is_allowed FROM user_permissions WHERE username = ? AND permission_key = ?");
    query.addBindValue(username);
    query.addBindValue(permissionKey);

    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }

    return -1;
}

bool AccessControl::getRoleDefaultPermission(Action action, Role role)
{
    if (role == Manager) {
        switch (action) {
        case AddCategory:
        case ManageDiscounts:
            return true;
        default:
            return false;
        }
    }

    if (role == Cashier) {
        switch (action) {
        case WithdrawCash:
        case DeleteTicket:
        case DeleteProduct:
            return true;
        default:
            return false;
        }
    }

    return false;
}

AccessControl::Role AccessControl::stringToRole(const QString& roleStr)
{
    if (roleStr.compare("Admin", Qt::CaseInsensitive) == 0) {
        return Admin;
    }
    if (roleStr.compare("Manager", Qt::CaseInsensitive) == 0) {
        return Manager;
    }
    if (roleStr.compare("Cashier", Qt::CaseInsensitive) == 0) {
        return Cashier;
    }
    if (roleStr.compare("User", Qt::CaseInsensitive) == 0) {
        return Cashier;
    }

    return Cashier;
}

QString AccessControl::roleToString(Role role)
{
    switch (role) {
    case Admin:
        return "Admin";
    case Manager:
        return "Manager";
    case Cashier:
        return "Cashier";
    default:
        return "Cashier";
    }
}

QString AccessControl::actionToPermissionKey(Action action)
{
    switch (action) {
    case WithdrawCash:
        return PERMISSION_WITHDRAW_CASH;
    case DeleteTicket:
        return PERMISSION_DELETE_TICKET;
    case DeleteProduct:
        return PERMISSION_DELETE_PRODUCT;
    case AddCategory:
        return PERMISSION_ADD_CATEGORY;
    case ManageDiscounts:
        return PERMISSION_MANAGE_DISCOUNTS;
    default:
        return QString();
    }
}

AccessControl::Action AccessControl::permissionKeyToAction(const QString& key)
{
    if (key == PERMISSION_WITHDRAW_CASH) return WithdrawCash;
    if (key == PERMISSION_DELETE_TICKET) return DeleteTicket;
    if (key == PERMISSION_DELETE_PRODUCT) return DeleteProduct;
    if (key == PERMISSION_ADD_CATEGORY) return AddCategory;
    if (key == PERMISSION_MANAGE_DISCOUNTS) return ManageDiscounts;
    return WithdrawCash;
}

QString AccessControl::roleToPermissionSuffix(Role role)
{
    switch (role) {
    case Manager:
        return "_manager";
    case Cashier:
        return "_cashier";
    default:
        return QString();
    }
}

bool AccessControl::isAdminRole(Role role)
{
    return role == Admin;
}

bool AccessControl::isAdminRole(const QString& roleStr)
{
    return roleStr.compare("Admin", Qt::CaseInsensitive) == 0;
}
