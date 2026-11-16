#ifndef REGISTERWINDOW_H
#define REGISTERWINDOW_H

#include <QMainWindow>
#include <QHash>
#include <QList>
#include <QPointer>
#include <QResizeEvent>
#include <QSet>
#include <QImage>
#include "DiscountHandler.h"
#include "VariantPickerDialog.h"
#include "CustomItemDialog.h"
#include "ProductButton.h"

class UserSession;
class LocalApi;
class LocalImages;
class ChangeNotifier;
class NotificationBell;
class QPushButton;
class QTimer;
class QLabel;

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
    int addedOrder; // rows stay in the order items were added

    CartItem()
        : price(0.0), cost(0.0), quantity(1), stock(0),
        isService(false), originalProductId(-1), variantId(-1)
    {
        static int nextOrder = 0;
        addedOrder = nextOrder++;
    }
};

/**
 * Main Register window
 * Handles POS operations: product display, cart management, sales
 */
class RegisterWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit RegisterWindow(UserSession* session, LocalApi* api,
                            LocalImages* images,
                            ChangeNotifier* notifier, QWidget* parent = nullptr);
    ~RegisterWindow();

    // Handles a "Register|flag|id" deep-link (from the Launcher navigation stream).
    void handleNavigation(const QString& action);

private slots:
    // Top bar actions
    void onLogoutClicked();

    // Product section
    void onSearchChanged(const QString& text);
    void onCategoryChanged(int index);
    void onSearchTimer();
    void onProductButtonClicked();
    void onCustomItemClicked();
    void onImageReady(const QString& filename, int maxSize, const QImage& image);

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
    QList<CartItem> cartInOrder() const;
    QList<int> cartKeysInOrder() const;
    void setCheckoutLabels(const QString& confirm, const QString& cancel);
    void updateCheckoutState();

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

    // Helpers
    void updateUserDisplay();
    void updateDailyIncome();
    void showTicketCompletedDialog(int ticketId);
    void printTicket(int ticketId);

protected:
    void resizeEvent(QResizeEvent* event) override;

    Ui::RegisterWindow* ui;

    // Core dependencies
    UserSession* m_session;
    LocalApi* m_api;
    LocalImages* m_images;
    ChangeNotifier* m_notifier;

    // Data
    QList<QHash<QString, QVariant>> m_categories;
    QList<QHash<QString, QVariant>> m_products;
    bool m_productsLoaded = false;
    bool m_loadedShowOutOfStock = true;
    QHash<int, CartItem> m_cart;  // Key = productId or variant offset
    QList<DiscountItem> m_discounts;

    // Cart panel
    void buildLayout();
    QLabel* m_cartCountLabel = nullptr;
    QWidget* m_cartEmptyHint = nullptr;
    QWidget* m_totalsDetail = nullptr;
    QLabel* m_subtotalLabel = nullptr;
    QLabel* m_discountLabel = nullptr;
    QString m_confirmText;   // set in the constructor, in the app language

    // Product buttons (for updating stock)
    QList<QWidget*> m_productButtons;

    // Product images for the current grid, keyed by filename. Filled by
    // displayProducts(); LocalImages serves them asynchronously (never on
    // the GUI thread), so the window stays responsive while they arrive.
    QHash<QString, QList<QPointer<ProductButton>>> m_buttonsByImage;
    QSet<QString> m_requestedImages;  // in flight or resolved (per run)

    // Resize fires continuously while dragging; rebuild the grid once the
    // size settles instead of once per event.
    QTimer* m_resizeRebuildTimer;

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

    // Notifications (derived from the data, read state per user)
    NotificationBell* m_bell = nullptr;
};

#endif // REGISTERWINDOW_H
