#ifndef FINANCIALUIMAP_H
#define FINANCIALUIMAP_H

class QLabel;
class QPushButton;
class QComboBox;
class QTableWidget;
class QGroupBox;

namespace Ui {
class FinancialWindow;
}

/**
 * Static UI element mapper for the Financial window.
 * Maps the UI elements from financialwindow.ui to static pointers.
 */
class FinancialUIMap
{
public:
    static void initialize(Ui::FinancialWindow* ui);

    // ── Top bar ───────────────────────────────────────────────────────────────
    static QLabel*       userLabel;
    static QPushButton*  notificationBtn;
    static QPushButton*  logoutBtn;

    // ── Tab 1 — Overview ──────────────────────────────────────────────────────
    static QComboBox*    monthSelect;
    static QComboBox*    yearSelect;
    static QGroupBox*    expensesBox;
    static QPushButton*  addExpenseBtn;
    static QPushButton*  deleteExpenseBtn;
    static QTableWidget* expensesTable;

    // ── Tab 2 — Yearly Summary ────────────────────────────────────────────────
    static QComboBox*    yearlySelect;
    static QTableWidget* yearlyTable;

private:
    FinancialUIMap() = delete;
    ~FinancialUIMap() = delete;
    FinancialUIMap(const FinancialUIMap&) = delete;
    FinancialUIMap& operator=(const FinancialUIMap&) = delete;
};

#endif // FINANCIALUIMAP_H
