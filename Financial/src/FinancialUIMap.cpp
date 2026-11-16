#include "FinancialUIMap.h"
#include "FinancialConstants.h"
#include "ui_financialwindow.h"
#include "StyleManager.h"
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QTableWidget>
#include <QGroupBox>
#include <QFont>
#include <QHeaderView>
#include <QIcon>
#include <QDate>
#include <QDebug>

// ── Static member initialization ──────────────────────────────────────────────
QLabel*       FinancialUIMap::userLabel       = nullptr;
QPushButton*  FinancialUIMap::notificationBtn = nullptr;
QPushButton*  FinancialUIMap::logoutBtn       = nullptr;
QComboBox*    FinancialUIMap::monthSelect     = nullptr;
QComboBox*    FinancialUIMap::yearSelect      = nullptr;
QGroupBox*    FinancialUIMap::expensesBox     = nullptr;
QPushButton*  FinancialUIMap::addExpenseBtn   = nullptr;
QPushButton*  FinancialUIMap::deleteExpenseBtn= nullptr;
QTableWidget* FinancialUIMap::expensesTable   = nullptr;
QComboBox*    FinancialUIMap::yearlySelect    = nullptr;
QTableWidget* FinancialUIMap::yearlyTable     = nullptr;

// ── Initialize ────────────────────────────────────────────────────────────────
// Texts are set by FinancialWindow::retranslateUi(), so they follow the app
// language.
void FinancialUIMap::initialize(Ui::FinancialWindow* ui)
{
    if (!ui) {
        qCritical() << "FinancialUIMap::initialize() called with null UI!";
        return;
    }

    // ── Top bar ───────────────────────────────────────────────────────────────
    userLabel       = ui->userLabel;
    notificationBtn = ui->notificationBtn;
    logoutBtn       = ui->logoutBtn;

    notificationBtn->setText("");
    notificationBtn->setIcon(QIcon(StyleManager::iconPath("notification")));
    notificationBtn->setIconSize(QSize(22, 22));
    notificationBtn->setFixedSize(36, 36);

    logoutBtn->setText("");
    logoutBtn->setIcon(QIcon(StyleManager::iconPath("logout")));
    logoutBtn->setIconSize(QSize(22, 22));
    logoutBtn->setFixedSize(36, 36);

    ui->tabwidget->setCurrentIndex(0);

    // ── Tab 1 — Overview ──────────────────────────────────────────────────────
    monthSelect      = ui->monthSelect;
    yearSelect       = ui->yearSelect;
    expensesBox      = ui->expensesBox;
    addExpenseBtn    = ui->addExpenseBtn;
    deleteExpenseBtn = ui->deleteExpenseBtn;
    expensesTable    = ui->expensesTable;

    // Month selector: 12 entries, names filled in by retranslateUi().
    monthSelect->clear();
    for (int i = 0; i < 12; ++i)
        monthSelect->addItem(QString(), i + 1);
    monthSelect->setCurrentIndex(QDate::currentDate().month() - 1);

    // Year selector
    yearSelect->clear();
    const int currentYear = QDate::currentDate().year();
    for (int y = currentYear - 3; y <= currentYear + 1; ++y)
        yearSelect->addItem(QString::number(y), y);
    yearSelect->setCurrentText(QString::number(currentYear));

    // One-time expenses table
    expensesTable->setColumnCount(2);
    expensesTable->verticalHeader()->setVisible(false);
    expensesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    expensesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    expensesTable->setSelectionMode(QAbstractItemView::SingleSelection);
    expensesTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    expensesTable->setShowGrid(false);
    expensesTable->setAlternatingRowColors(true);

    addExpenseBtn->setProperty("btnStyle", "add");
    addExpenseBtn->style()->unpolish(addExpenseBtn);
    addExpenseBtn->style()->polish(addExpenseBtn);

    deleteExpenseBtn->setProperty("btnStyle", "delete");
    deleteExpenseBtn->style()->unpolish(deleteExpenseBtn);
    deleteExpenseBtn->style()->polish(deleteExpenseBtn);

    // ── Tab 2 — Yearly Summary ────────────────────────────────────────────────
    yearlySelect   = ui->yearlySelect;
    yearlyTable    = ui->yearlyTable;

    yearlySelect->clear();
    for (int y = currentYear - 3; y <= currentYear + 1; ++y)
        yearlySelect->addItem(QString::number(y), y);
    yearlySelect->setCurrentText(QString::number(currentYear));

    // Yearly table — 13 rows (12 months + total)
    yearlyTable->setRowCount(13);
    yearlyTable->setColumnCount(FinancialConstants::YEAR_COLUMNS);
    for (int col = 0; col < FinancialConstants::YEAR_COLUMNS; ++col)
        yearlyTable->setHorizontalHeaderItem(col, new QTableWidgetItem);
    yearlyTable->verticalHeader()->setVisible(false);
    yearlyTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    yearlyTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    yearlyTable->setSelectionMode(QAbstractItemView::SingleSelection);
    yearlyTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    yearlyTable->verticalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    yearlyTable->setShowGrid(true);
    yearlyTable->setAlternatingRowColors(true);

    const Qt::Alignment numberAlign = Qt::AlignRight | Qt::AlignVCenter;
    const Qt::Alignment textAlign = Qt::AlignLeft | Qt::AlignVCenter;
    for (int row = 0; row < 12; ++row) {
        QTableWidgetItem* monthItem = new QTableWidgetItem;
        monthItem->setTextAlignment(textAlign);
        yearlyTable->setItem(row, FinancialConstants::COL_YEAR_MONTH, monthItem);
        for (int col = 1; col < FinancialConstants::YEAR_COLUMNS; ++col) {
            QTableWidgetItem* valItem = new QTableWidgetItem("—");
            valItem->setTextAlignment(numberAlign);
            yearlyTable->setItem(row, col, valItem);
        }
    }
    QTableWidgetItem* totalLabel = new QTableWidgetItem;
    totalLabel->setTextAlignment(textAlign);
    QFont totalFont = totalLabel->font();
    totalFont.setBold(true);
    totalLabel->setFont(totalFont);
    yearlyTable->setItem(12, 0, totalLabel);
    for (int col = 1; col < FinancialConstants::YEAR_COLUMNS; ++col) {
        QTableWidgetItem* valItem = new QTableWidgetItem("—");
        valItem->setTextAlignment(numberAlign);
        valItem->setFont(totalFont);
        yearlyTable->setItem(12, col, valItem);
    }

    qDebug() << "FinancialUIMap initialized successfully";
}
