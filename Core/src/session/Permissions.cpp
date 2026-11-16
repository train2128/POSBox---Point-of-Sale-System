#include "Permissions.h"
#include "LocalApi.h"
#include <QList>

namespace Permissions {

bool can(LocalApi* api, const QString& username, const QString& role,
         const QString& permissionKey)
{
    if (username.isEmpty())
        return false;
    if (role.compare("Admin", Qt::CaseInsensitive) == 0)
        return true;
    if (api) {
        bool allowed = api->permission(username, permissionKey);
        if (api->ok())
            return allowed; // user override, else the role default
        return false; // database error: fail closed
    }
    // No channel (tests/tools): static role defaults.
    // Mirrors AccessControl::getRoleDefaultPermission.
    if (role.compare("Manager", Qt::CaseInsensitive) == 0) {
        return permissionKey == "add_category" || permissionKey == "manage_discounts";
    }
    if (role.compare("Cashier", Qt::CaseInsensitive) == 0) {
        return permissionKey == "withdraw_cash" || permissionKey == "delete_ticket"
            || permissionKey == "delete_product";
    }
    return false;
}

} // namespace Permissions
