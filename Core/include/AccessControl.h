#ifndef ACCESSCONTROL_H
#define ACCESSCONTROL_H

#include <QString>
#include <QSqlDatabase>

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
        Cashier  // Maps to both "Cashier" and "User" in database
    };

    enum Page {
        RegisterPage,
        TicketsPage,
        InventoryPage,
        SettingsPage,
        FinancesPage
    };

    enum Action {
        WithdrawCash,

        DeleteTicket,

        DeleteProduct,
        AddCategory,
        ManageDiscounts
    };

    static constexpr const char* PERMISSION_WITHDRAW_CASH    = "withdraw_cash";
    static constexpr const char* PERMISSION_DELETE_TICKET   = "delete_ticket";
    static constexpr const char* PERMISSION_DELETE_PRODUCT   = "delete_product";
    static constexpr const char* PERMISSION_ADD_CATEGORY     = "add_category";
    static constexpr const char* PERMISSION_MANAGE_DISCOUNTS = "manage_discounts";

    static bool canAccessPage(Page page, Role role);
    static bool canAccessPage(const QString& username, Page page, QSqlDatabase db = QSqlDatabase());

    static bool canPerformAction(Action action, Role role);
    static bool canPerformAction(const QString& username, Action action, QSqlDatabase db = QSqlDatabase());
    static bool canPerformAction(const QString& username, const QString& permissionKey, QSqlDatabase db = QSqlDatabase());

    static Role stringToRole(const QString& roleStr);
    static QString roleToString(Role role);

    static QString actionToPermissionKey(Action action);
    static Action permissionKeyToAction(const QString& key);

    static QString roleToPermissionSuffix(Role role);

    static bool isAdminRole(Role role);
    static bool isAdminRole(const QString& roleStr);

private:
    static bool getUserPermissionOverride(const QString& username, const QString& permissionKey, QSqlDatabase db);
    static bool getRoleDefaultPermission(Action action, Role role);

    AccessControl() = delete;
    ~AccessControl() = delete;
    AccessControl(const AccessControl&) = delete;
    AccessControl& operator=(const AccessControl&) = delete;
};

#endif // ACCESSCONTROL_H
