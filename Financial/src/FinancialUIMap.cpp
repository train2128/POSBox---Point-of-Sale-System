#include "FinancialUIMap.h"
#include "FinancialConstants.h"
#include "ui_financialwindow.h"
#include "StyleManager.h"
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QTableWidget>
#include <QGroupBox>
#include <QHeaderView>
#include <QIcon>
#include <QDate>
#include <QLocale>
#include <QDebug>

// ── Static member initialization ──────────────────────────────────────────────

QLabel*       FinancialUIMap::userLabel       = nullptr;
QPushButton*  FinancialUIMap::notificationBtn = nullptr;
QPushButton*  FinancialUIMap::logoutBtn       = nullptr;

QComboBox*    FinancialUIMap::monthSelect     = nullptr;
QComboBox*    FinancialUIMap::yearSelect      = nullptr;
QTableWidget* FinancialUIMap::monthlyTable    = nullptr;
QTableWidget* FinancialUIMap::weeklyTable     = nullptr;
QTableWidget* FinancialUIMap::topSalesTable  = nullptr;
QGroupBox*    FinancialUIMap::expensesBox     = nullptr;
QPushButton*  FinancialUIMap::addExpenseBtn   = nullptr;
QPushButton*  FinancialUIMap::deleteExpenseBtn= nullptr;
QTableWidget* FinancialUIMap::expensesTable   = nullptr;

QTableWidget* FinancialUIMap::yearlyTable     = nullptr;

// ── Initialize ────────────────────────────────────────────────────────────────

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
    notificationBtn->setToolTip("");

    logoutBtn->setText("");
    logoutBtn->setIcon(QIcon(StyleManager::iconPath("logout")));
    logoutBtn->setIconSize(QSize(22, 22));
    logoutBtn->setFixedSize(36, 36);
    logoutBtn->setToolTip("");

    // ── Tab labels ────────────────────────────────────────────────────────────
    ui->tabwidget->setTabText(0, "");
    ui->tabwidget->setTabText(1, "");
    ui->tabwidget->setTabText(2, "");
    ui->tabwidget->setTabText(3, "");
    ui->tabwidget->setCurrentIndex(0);

    // ── Tab 1 — Overview ──────────────────────────────────────────────────────
    monthSelect      = ui->monthSelect;
    yearSelect       = ui->yearSelect;
    monthlyTable     = ui->monthlyTable;
    weeklyTable      = ui->weeklyTable;
    topSalesTable    = ui->topSalesTable;
    expensesBox      = ui->expensesBox;
    addExpenseBtn    = ui->addExpenseBtn;
    deleteExpenseBtn = ui->deleteExpenseBtn;
    expensesTable    = ui->expensesTable;

    // Month selector
    monthSelect->clear();
    for (int i = 0; i < 12; ++i)
        monthSelect->addItem(QLocale::system().monthName(i + 1, QLocale::LongFormat), i + 1);
    monthSelect->setCurrentIndex(QDate::currentDate().month() - 1);

    // Year selector
    yearSelect->clear();
    const int currentYear = QDate::currentDate().year();
    for (int y = currentYear - 3; y <= currentYear + 1; ++y)
        yearSelect->addItem(QString::number(y), y);
    yearSelect->setCurrentText(QString::number(currentYear));

    // ── Monthly summary table — 1 row, 5 columns (header is the title row) ───
    monthlyTable->setRowCount(1);
    monthlyTable->setColumnCount(5);
    monthlyTable->setHorizontalHeaderLabels({
        "", "", "", "", ""
    });
    monthlyTable->verticalHeader()->setVisible(false);
    monthlyTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    monthlyTable->setSelectionMode(QAbstractItemView::NoSelection);
    monthlyTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    monthlyTable->setShowGrid(true);
    monthlyTable->setMaximumHeight(65);

    for (int col = 0; col < 5; ++col) {
        QTableWidgetItem* item = new QTableWidgetItem("—");
        item->setTextAlignment(Qt::AlignCenter);
        monthlyTable->setItem(0, col, item);
    }

    // ── Weekly table — 8 rows (Mon-Sun + Total), 3 columns ───────────────────
    weeklyTable->setRowCount(8);
    weeklyTable->setColumnCount(3);
    weeklyTable->setHorizontalHeaderLabels({"", "", ""});
    weeklyTable->verticalHeader()->setVisible(false);
    weeklyTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    weeklyTable->setSelectionMode(QAbstractItemView::NoSelection);
    weeklyTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    weeklyTable->setShowGrid(true);

    // Day names: Sunday=7, Monday=1, ..., Saturday=6 per Qt locale
    const int dayMapping[] = {7, 1, 2, 3, 4, 5, 6};
    for (int row = 0; row < 7; ++row) {
        QTableWidgetItem* dayItem = new QTableWidgetItem(QLocale::system().dayName(dayMapping[row], QLocale::LongFormat));
        dayItem->setTextAlignment(Qt::AlignCenter);
        weeklyTable->setItem(row, FinancialConstants::COL_WEEKLY_DAY, dayItem);
        for (int col = 1; col < 3; ++col) {
            QTableWidgetItem* valItem = new QTableWidgetItem("—");
            valItem->setTextAlignment(Qt::AlignCenter);
            weeklyTable->setItem(row, col, valItem);
        }
    }
    // Total row
    {
        QTableWidgetItem* totalItem = new QTableWidgetItem("");
        totalItem->setTextAlignment(Qt::AlignCenter);
        weeklyTable->setItem(7, FinancialConstants::COL_WEEKLY_DAY, totalItem);
        for (int col = 1; col < 3; ++col) {
            QTableWidgetItem* valItem = new QTableWidgetItem("—");
            valItem->setTextAlignment(Qt::AlignCenter);
            weeklyTable->setItem(7, col, valItem);
        }
    }

    // ── Top Sales table — 3 rows, 3 columns ─────────────────────────────────
    topSalesTable->setRowCount(3);
    topSalesTable->setColumnCount(3);
    topSalesTable->setHorizontalHeaderLabels({"", "", ""});
    topSalesTable->verticalHeader()->setVisible(false);
    topSalesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    topSalesTable->setSelectionMode(QAbstractItemView::NoSelection);
    topSalesTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    topSalesTable->setShowGrid(true);

    for (int row = 0; row < 3; ++row) {
        QTableWidgetItem* rankItem = new QTableWidgetItem(QString("#%1").arg(row + 1));
        rankItem->setTextAlignment(Qt::AlignCenter);
        topSalesTable->setItem(row, FinancialConstants::COL_TOPSALES_RANK, rankItem);
        for (int col = 1; col < 3; ++col) {
            QTableWidgetItem* valItem = new QTableWidgetItem("—");
            valItem->setTextAlignment(Qt::AlignCenter);
            topSalesTable->setItem(row, col, valItem);
        }
    }

    // GroupBox titles
    expensesBox->setTitle("");

    // One-time expenses table
    expensesTable->setColumnCount(3);
    expensesTable->setHorizontalHeaderLabels({"", "", ""});
    expensesTable->verticalHeader()->setVisible(false);
    expensesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    expensesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    expensesTable->setSelectionMode(QAbstractItemView::SingleSelection);
    expensesTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    expensesTable->setShowGrid(false);
    expensesTable->setAlternatingRowColors(true);

    addExpenseBtn->setText("");
    addExpenseBtn->setProperty("btnStyle", "add");
    addExpenseBtn->style()->unpolish(addExpenseBtn);
    addExpenseBtn->style()->polish(addExpenseBtn);

    deleteExpenseBtn->setText("");
    deleteExpenseBtn->setProperty("btnStyle", "delete");
    deleteExpenseBtn->style()->unpolish(deleteExpenseBtn);
    deleteExpenseBtn->style()->polish(deleteExpenseBtn);

    // ── Tab 2 — Yearly Summary ───────────────────────────────────────────────────
    yearlyTable    = ui->yearlyTable;

    // Yearly table — 13 rows (12 months + total), 5 columns
    yearlyTable->setRowCount(13);
    yearlyTable->setColumnCount(5);
    yearlyTable->setHorizontalHeaderLabels({
        "", "", "", "", ""
    });
    yearlyTable->verticalHeader()->setVisible(false);
    yearlyTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    yearlyTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    yearlyTable->setSelectionMode(QAbstractItemView::SingleSelection);
    yearlyTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    yearlyTable->setShowGrid(true);
    yearlyTable->setAlternatingRowColors(true);

    for (int row = 0; row < 12; ++row) {
        QTableWidgetItem* monthItem = new QTableWidgetItem(QLocale::system().monthName(row + 1, QLocale::LongFormat));
        monthItem->setTextAlignment(Qt::AlignCenter);
        yearlyTable->setItem(row, FinancialConstants::COL_YEAR_MONTH, monthItem);
        for (int col = 1; col < 5; ++col) {
            QTableWidgetItem* valItem = new QTableWidgetItem("—");
            valItem->setTextAlignment(Qt::AlignCenter);
            yearlyTable->setItem(row, col, valItem);
        }
    }
    QTableWidgetItem* totalLabel = new QTableWidgetItem("");
    QFont totalFont = totalLabel->font();
    totalFont.setBold(true);
    totalFont.setPointSize(totalFont.pointSize() + 2);
    totalLabel->setFont(totalFont);
    totalLabel->setTextAlignment(Qt::AlignCenter);
    yearlyTable->setItem(12, 0, totalLabel);
    for (int col = 1; col < 5; ++col) {
        QTableWidgetItem* valItem = new QTableWidgetItem("—");
        valItem->setFont(totalFont);
        valItem->setTextAlignment(Qt::AlignCenter);
        yearlyTable->setItem(12, col, valItem);
    }

    qDebug() << "FinancialUIMap initialized successfully";
}
