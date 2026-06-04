#ifndef REGISTERWINDOW_H
#define REGISTERWINDOW_H

#include <QMainWindow>
#include <QHash>
#include <QList>
#include <QResizeEvent>
#include "DiscountHandler.h"
#include "VariantPickerDialog.h"
#include "CustomItemDialog.h"

class UserSession;
class DatabaseManager;
class ChangeNotifier;
class NotificationManager;
class NotificationPanel;
class QPushButton;
class QTimer;

namespace Ui {
class RegisterWindow;
}

/**
 * Cart item data structure
 */
struct CartItem {
    QString name;
    double price;
    double cost;
    int quantity;
    int stock;       // max available stock (0 = unlimited for services/custom)
    bool isService;
    int originalProductId;
    int variantId;  // -1 if no variant

    CartItem()
        : price(0.0), cost(0.0), quantity(1), stock(0),
        isService(false), originalProductId(-1), variantId(-1) {}
};

/**
 * Main Register window
 * Handles POS operations: product display, cart management, sales
 */
class RegisterWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit RegisterWindow(UserSession* session, DatabaseManager* dbManager,
                            ChangeNotifier* notifier, QWidget* parent = nullptr);
    ~RegisterWindow();

    void checkPendingEditRequest();

private slots:
    // Top bar actions
    void onTicketsClicked();
    void onNotificationClicked();
    void onLogoutClicked();

    // Product section
    void onSearchChanged(const QString& text);
    void onCategoryChanged(int index);
    void onSearchTimer();
    void onProductButtonClicked();
    void onCustomItemClicked();

    // Cart section
    void onCartQuantityChanged(int cartKey, int newQuantity);
    void onCartItemRemove(int cartKey);
    void onDiscountClicked();
    void onDiscountRemove(int discountIndex);
    void onOpenTicketsClicked();
    void onWithdrawClicked();
    void onConfirmClicked();
    void onCancelClicked();

signals:
    void logoutRequested();

private:
    // Initialization
    void setupUI();
    void setupConnections();
    void loadCategories();
    void loadProducts();

    // Product display
    void displayProducts();
    void clearProductGrid();
    QList<QHash<QString, QVariant>> filterProducts();
    int calculateColumnCount() const;

    // Cart management
    void addToCart(int productId);
    void addVariantToCart(int productId, const VariantOption& variant);
    void updateCartDisplay();
    void updateCartTotal();
    void clearCart();

    // Open ticket button state
    void updateOpenTicketButton();

    // Transaction
    bool validateTransaction();
    void processTransaction();
    void saveEditedTransaction();

    // Open ticket operations
    void saveAsOpenTicket();
    void completeOpenTicket(int ticketId);
    void enterOpenTicketEditMode(int ticketId);

    // Edit mode
    void enterEditMode(int ticketId);
    void exitEditMode();
    void adjustStockDiff(const QHash<int, int>& oldQtys, const QHash<int, int>& newQtys);
    void adjustStockDiff(const QHash<int, int>& oldQtys, const QHash<int, int>& newQtys, bool isVariant);

    // Helpers
    void updateUserDisplay();
    void deductStockForTicket(int ticketId);
    void showTicketCompletedDialog(int ticketId);
    void printTicket(int ticketId);
    void updateProductOutOfStock(int productId, bool hasVariants);
    void updateVariantOutOfStock(int variantId);

protected:
    void resizeEvent(QResizeEvent* event) override;

    Ui::RegisterWindow* ui;

    // Core dependencies
    UserSession* m_session;
    DatabaseManager* m_db;
    ChangeNotifier* m_notifier;

    // Data
    QList<QHash<QString, QVariant>> m_categories;
    QList<QHash<QString, QVariant>> m_products;
    QHash<int, CartItem> m_cart;  // Key = productId or variant offset
    QList<DiscountItem> m_discounts;

    // Product buttons (for updating stock)
    QList<QWidget*> m_productButtons;

    // Search
    QTimer* m_searchTimer;
    QString m_currentSearchText;
    int m_currentCategoryFilter;  // -1 = all categories

    // Cart totals
    double m_cartTotal;

    // Custom item key counter — decrements from -1 to avoid collision with product/variant IDs
    int m_customItemCounter;

    // Edit mode state (completed ticket edit via Tickets app)
    bool m_editMode;
    int  m_editTicketId;

    // Open ticket edit state (editing an OPEN ticket from the dialog)
    bool m_openTicketEditMode;
    int  m_openTicketEditId;

    // Saved open ticket metadata (used when re-saving in open ticket edit mode)
    QString m_openTicketCustomerName;
    double  m_openTicketAdvance;
    QString m_openTicketDescription;

    // Notification system
    NotificationManager* m_notificationManager;
    NotificationPanel* m_notificationPanel;
    void updateNotificationBadge();
};

#endif // REGISTERWINDOW_H
