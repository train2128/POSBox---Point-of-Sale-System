#ifndef ACCESSCONTROL_H
#define ACCESSCONTROL_H

#include <QString>

/**
 * Static access control system
 * All methods are static - no instances needed
 */
class AccessControl
{
public:
    enum Role {
        Admin,
        Manager,
        Cashier  // Also covers the older "User" role name
    };

    enum Page {
        RegisterPage,
        TicketsPage,
        InventoryPage,
        SettingsPage,
        FinancesPage
    };

    static constexpr const char* PERMISSION_WITHDRAW_CASH    = "withdraw_cash";
    static constexpr const char* PERMISSION_DELETE_TICKET   = "delete_ticket";
    static constexpr const char* PERMISSION_DELETE_PRODUCT   = "delete_product";
    static constexpr const char* PERMISSION_ADD_CATEGORY     = "add_category";
    static constexpr const char* PERMISSION_MANAGE_DISCOUNTS = "manage_discounts";

    static bool canAccessPage(Page page, Role role);

    static Role stringToRole(const QString& roleStr);

    static bool isAdminRole(const QString& roleStr);

private:
    AccessControl() = delete;
    ~AccessControl() = delete;
    AccessControl(const AccessControl&) = delete;
    AccessControl& operator=(const AccessControl&) = delete;
};

#endif // ACCESSCONTROL_H
