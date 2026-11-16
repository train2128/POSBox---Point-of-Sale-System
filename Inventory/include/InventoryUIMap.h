#ifndef INVENTORYUIMAP_H
#define INVENTORYUIMAP_H

class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;

namespace Ui {
class MainWindow;
}

/**
 * Static UI element mapper for InventoryWindow
 * Maps UI elements from Invenorywindow.ui to static pointers
 */
class InventoryUIMap
{
public:
    /**
     * Initialize all UI element pointers from the .ui file
     * @param ui - UI object from InventoryWindow
     */
    static void initialize(Ui::MainWindow* ui);

    // Header
    static QLabel*   headerTitle;
    static QLabel*   headerSubtitle;

    // KPI strip
    static QPushButton* productsCountLabel;
    static QPushButton* outOfStockLabel;
    static QPushButton* lowStockLabel;

    // Tab buttons (replaces the old QComboBox tableSelect)
    static QPushButton*  productsTab;
    static QPushButton*  categoriesTab;
    static QPushButton*  discountsTab;

    // Search + actions
    static QLineEdit*    searchBox;
    static QPushButton*  addBtn;
    static QPushButton*  editBtn;
    static QPushButton*  deleteBtn;
    static QPushButton*  notificationBtn;
    static QPushButton*  logoutBtn;

    // Footer
    static QLabel*   footerLabel;

    // Main table
    static QTableWidget* mainTable;

private:
    // No instances allowed
    InventoryUIMap() = delete;
    ~InventoryUIMap() = delete;
    InventoryUIMap(const InventoryUIMap&) = delete;
    InventoryUIMap& operator=(const InventoryUIMap&) = delete;
};

#endif // INVENTORYUIMAP_H
