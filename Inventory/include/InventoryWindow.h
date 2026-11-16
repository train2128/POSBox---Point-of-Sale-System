#ifndef INVENTORYWINDOW_H
#define INVENTORYWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QColor>

class UserSession;
class LocalApi;
class LocalImages;
class ChangeNotifier;
class NotificationBell;
class QTableWidgetItem;
class QResizeEvent;
class QLabel;
class QVBoxLayout;
class QHBoxLayout;
class QImage;

namespace Ui {
class MainWindow;
}

/**
 * Main Inventory window
 * Handles products, categories, and discounts management
 */
class InventoryWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit InventoryWindow(UserSession* session, LocalApi* api,
                             LocalImages* images, ChangeNotifier* notifier,
                             QWidget* parent = nullptr);
    ~InventoryWindow();

    // Handles a "Inventory|flag|id" deep-link (from the Launcher navigation stream).
    void handleNavigation(const QString& action);

protected:
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onSearchChanged(const QString& text);
    void onSearchTimer();
    void onAddClicked();
    void onEditClicked();
    void onDeleteClicked();
    void onLogoutClicked();
    void onSelectionChanged();
    void onCellDoubleClicked(int row, int column);
    void onCellChanged(int row, int column);
    void onProductImageReady(const QString& filename, int maxSize, const QImage& image);

    // Called on every flash timer tick to toggle out-of-stock row text color
    void onFlashTick();

signals:
    void logoutRequested();

private:
    enum class StockFilter { All, OutOfStock, LowStock };

    // Setup — Proposal A UI
    void setupUI();
    void setupConnections();
    void setupHeader(QVBoxLayout* rootLayout);
    void setupTabButtons(QHBoxLayout* actionLayout);
    void setupActionBar(QVBoxLayout* rootLayout);
    void setupTableHeaders(int tableIndex);
    void applyProductColumnRatios();
    void applyCategoryColumnRatio();

    // Tab handling (replaces QComboBox tableSelect)
    void onTabClicked(int index);

    // KPI strip updates
    void updateKPIs(int totalProducts, int outOfStock, int lowStock);
    void setStockFilter(StockFilter filter, int lowStockThreshold);
    void applyCurrentStockFilter();
    void updateStockFilterButtons();

    // Data loading
    void loadCurrentTable();
    void loadProducts(const QString& search = QString());
    void loadCategories(const QString& search = QString());
    void loadDiscounts(const QString& search = QString());

    // Products CRUD
    void addProduct();
    void editProduct(int row);
    void deleteProduct(int row);

    // Categories CRUD
    void addCategory();
    void editCategory(int row);
    void deleteCategory(int row);

    // Discounts CRUD
    void addDiscount();
    void editDiscount(int row);
    void deleteDiscount(int row);

    // Stock color helpers
    // Applies yellow / red text coloring to all product rows based on stock level.
    // Called once after loadProducts() finishes populating the table.
    void applyStockColors();

    // Starts the flash timer if any out-of-stock rows exist; stops and nulls it otherwise.
    // Safe to call unconditionally after every loadProducts().
    void updateFlashTimer();

    // Returns the theme-appropriate default text color string (used as flash "off" state).
    QString defaultTextColor() const;

    // Returns a distinct color for a letter badge based on the letter.
    QColor badgeColorForLetter(const QString& letter) const;

    // Helpers
    int getSelectedRow() const;
    int getProductIdFromRow(int row) const;
    int getCategoryIdFromRow(int row) const;
    int getDiscountIdFromRow(int row) const;

    Ui::MainWindow* ui;

    UserSession*     m_session;
    LocalApi*       m_api;
    LocalImages*    m_images;
    ChangeNotifier*  m_notifier;

    // Search debounce timer — always allocated in constructor
    QTimer* m_searchTimer;
    QString m_currentSearch;

    // Flash timer — allocated only when out-of-stock rows are present, destroyed otherwise
    QTimer* m_flashTimer;

    // Current flash state: true = show red, false = show default text color
    bool m_flashState;

    int  m_currentTable;
    QWidget* m_tabBar = nullptr;
    StockFilter m_stockFilter;
    int m_stockFilterLowThreshold;
    bool m_isEditingStock;
    int  m_editingStockRow;
    bool m_blockCellChanged;
    bool m_reloadPending = false;   // a refresh that waits until the stock editor closes
    void recountStockKpis();

    // Notification system
    NotificationBell* m_bell = nullptr;
    void applyStockFilter(const QString& flag);
};

#endif // INVENTORYWINDOW_H
