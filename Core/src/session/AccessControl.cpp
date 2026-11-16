#include "AccessControl.h"

bool AccessControl::canAccessPage(Page page, Role role)
{
    if (role == Admin) {
        return true;
    }

    switch (page) {
    case RegisterPage:
    case TicketsPage:
        return true;

    case InventoryPage:
    case FinancesPage:
        return role == Manager;

    case SettingsPage:
    default:
        return false;
    }
}

AccessControl::Role AccessControl::stringToRole(const QString& roleStr)
{
    if (roleStr.compare("Admin", Qt::CaseInsensitive) == 0) {
        return Admin;
    }
    if (roleStr.compare("Manager", Qt::CaseInsensitive) == 0) {
        return Manager;
    }
    return Cashier;  // "Cashier", "User" and anything unknown
}

bool AccessControl::isAdminRole(const QString& roleStr)
{
    return roleStr.compare("Admin", Qt::CaseInsensitive) == 0;
}
