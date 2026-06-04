#ifndef INVENTORYUIMAP_H
#define INVENTORYUIMAP_H

class QComboBox;
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

    // Top bar elements
    static QComboBox*    tableSelect;
    static QLineEdit*    searchBox;
    static QPushButton*  addBtn;
    static QPushButton*  editBtn;
    static QPushButton*  deleteBtn;
    static QPushButton*  logoutBtn;

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
