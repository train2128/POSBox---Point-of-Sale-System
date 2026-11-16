#ifndef UIMAP_H
#define UIMAP_H

class QLabel;
class QPushButton;
class QLineEdit;
class QComboBox;
class QGridLayout;
class QVBoxLayout;
class QScrollArea;

namespace Ui {
class RegisterWindow;
}

/**
 * Static UI element mapper
 * Maps UI elements from .ui file to static pointers for easy access
 * Handles ONLY UI connections (signal to slot connections)
 */
class UIMap
{
public:
    /**
     * Initialize all UI element pointers from the .ui file
     * Sets up UI-level signal/slot connections
     * @param ui - UI object from RegisterWindow
     */
    static void initialize(Ui::RegisterWindow* ui);

    // Top bar elements
    static QLabel* userLabel;
    static QLabel* incomeLabel;
    static QPushButton* notificationBtn;
    static QPushButton* logoutBtn;

    // Products section (left side)
    static QLineEdit* searchBar;
    static QComboBox* filterBox;
    static QPushButton* customItemBtn;
    static QGridLayout* itemsGridLayout;

    // Cart section (right side)
    static QScrollArea* cart;
    static QVBoxLayout* cartLayout;
    static QLabel* totalLabel;
    static QPushButton* openTicketsBtn;
    static QPushButton* discountBtn;
    static QPushButton* withdrawBtn;
    static QPushButton* confirmBtn;
    static QPushButton* cancelBtn;

private:
    // No instances allowed
    UIMap() = delete;
    ~UIMap() = delete;
    UIMap(const UIMap&) = delete;
    UIMap& operator=(const UIMap&) = delete;
};

#endif // UIMAP_H
