#ifndef PERMISSIONS_H
#define PERMISSIONS_H

#include <QString>

class LocalApi;

// Permission check: Admin always allowed, then the
// user_permissions override row, then role defaults. Mirrors
// AccessControl semantics.
namespace Permissions {
bool can(LocalApi* api, const QString& username, const QString& role,
         const QString& permissionKey);
}

#endif // PERMISSIONS_H
