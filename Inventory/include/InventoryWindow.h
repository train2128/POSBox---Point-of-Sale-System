#ifndef INVENTORYWINDOW_H
#define INVENTORYWINDOW_H

#include <QMainWindow>
#include <QTimer>

class UserSession;
class DatabaseManager;
class ChangeNotifier;
class QTableWidgetItem;
class QResizeEvent;

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
    explicit InventoryWindow(UserSession* session, DatabaseManager* dbManager,
                             ChangeNotifier* notifier, QWidget* parent = nullptr);
    ~InventoryWindow();

    void retranslateUi();

protected:
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onTableSelectChanged(int index);
    void onSearchChanged(const QString& text);
    void onSearchTimer();
    void onAddClicked();
    void onEditClicked();
    void onDeleteClicked();
    void onLogoutClicked();
    void onSelectionChanged();
    void onCellDoubleClicked(int row, int column);
    void onCellChanged(int row, int column);

    // Called on every flash timer tick to toggle out-of-stock row text color
    void onFlashTick();

signals:
    void logoutRequested();

private:
    // Setup
    void setupUI();
    void setupConnections();
    void setupTableHeaders(int tableIndex);
    void applyCategoryColumnRatio();

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

    // Helpers
    int getSelectedRow() const;
    int getProductIdFromRow(int row) const;
    int getCategoryIdFromRow(int row) const;
    int getDiscountIdFromRow(int row) const;

    Ui::MainWindow* ui;

    UserSession*     m_session;
    DatabaseManager* m_db;
    ChangeNotifier*  m_notifier;

    // Search debounce timer — always allocated in constructor
    QTimer* m_searchTimer;
    QString m_currentSearch;

    // Flash timer — allocated only when out-of-stock rows are present, destroyed otherwise
    QTimer* m_flashTimer;

    // Current flash state: true = show red, false = show default text color
    bool m_flashState;

    int  m_currentTable;
    bool m_isEditingStock;
    int  m_editingStockRow;
    bool m_blockCellChanged;
};

#endif // INVENTORYWINDOW_H
