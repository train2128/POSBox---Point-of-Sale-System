#include "FinancialWindow.h"
#include "ui_financialwindow.h"
#include "FinancialUIMap.h"
#include "FinancialConstants.h"
#include "DatabaseManager.h"
#include "QueryRepository.h"
#include "SessionManager.h"
#include "StyleManager.h"
#include "UserSession.h"
#include "CommonConfig.h"
#include "Prefixes.h"
#include "NotificationManager.h"
#include "NotificationPanel.h"
#include "AccessControl.h"
#include "LogoutManager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QInputDialog>
#include <QMessageBox>
#include <QDate>
#include <QDebug>

FinancialWindow::FinancialWindow(UserSession* session,
                                 DatabaseManager* db,
                                 QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::FinancialWindow)
    , m_session(session)
    , m_db(db)
    , m_notificationManager(nullptr)
    , m_notificationPanel(nullptr)
{
    ui->setupUi(this);

    StyleManager::applyGlobalStyles(this, m_db->database());
    FinancialUIMap::initialize(ui);

    // Setup notification system
    m_notificationManager = new NotificationManager(db, this);
    m_notificationManager->checkAndGenerate();
    connect(m_notificationManager, &NotificationManager::notificationsChanged, this, &FinancialWindow::updateNotificationBadge);
    updateNotificationBadge();

    setupConnections();
    updateUserDisplay();

    // Load all data
    loadMonthlySummary();
    loadWeeklyTable();
    loadTopSalesTable();
    loadOneTimeExpenses();
    loadYearlyTable();

    showMaximized();
}

FinancialWindow::~FinancialWindow()
{
    delete ui;
}

void FinancialWindow::onNotificationClicked()
{
    if (!m_notificationPanel) {
        QString username = m_session ? m_session->username() : "";
        m_notificationPanel = new NotificationPanel(m_notificationManager, username, m_db->database(), this);
    }

    if (m_notificationPanel->isVisible()) {
        m_notificationPanel->hide();
    } else {
        QPoint btnPos = FinancialUIMap::notificationBtn->mapToGlobal(QPoint(0, 0));
        m_notificationPanel->move(btnPos.x() - m_notificationPanel->width() + FinancialUIMap::notificationBtn->width(), 
                                btnPos.y() + FinancialUIMap::notificationBtn->height());
        m_notificationPanel->show();
    }
}

void FinancialWindow::updateNotificationBadge()
{
    if (!FinancialUIMap::notificationBtn) return;

    int count = m_notificationManager ? m_notificationManager->unreadCount() : 0;

    if (count > 0) {
        FinancialUIMap::notificationBtn->setIcon(QIcon(StyleManager::iconPath("notification")));
        FinancialUIMap::notificationBtn->setStyleSheet(
            "QPushButton { background-color: #4A90E2; border-radius: 18px; }");
    } else {
        FinancialUIMap::notificationBtn->setIcon(QIcon(StyleManager::iconPath("notification")));
        FinancialUIMap::notificationBtn->setStyleSheet("");
    }
}

// ============================================================================
// SETUP
// ============================================================================

void FinancialWindow::setupConnections()
{
    // Top bar
    connect(FinancialUIMap::notificationBtn, &QPushButton::clicked,
            this, &FinancialWindow::onNotificationClicked);
    connect(FinancialUIMap::logoutBtn, &QPushButton::clicked,
            this, &FinancialWindow::onLogoutClicked);

    // Tab 1
    connect(FinancialUIMap::monthSelect, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FinancialWindow::onMonthYearChanged);
    connect(FinancialUIMap::yearSelect,  QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FinancialWindow::onMonthYearChanged);
    connect(FinancialUIMap::addExpenseBtn,    &QPushButton::clicked,
            this, &FinancialWindow::onAddExpenseClicked);
    connect(FinancialUIMap::deleteExpenseBtn, &QPushButton::clicked,
            this, &FinancialWindow::onDeleteExpenseClicked);

    // Tab 2
    connect(FinancialUIMap::yearlyTable, &QTableWidget::cellDoubleClicked,
            this, &FinancialWindow::onYearlyTableDoubleClicked);
}

void FinancialWindow::updateUserDisplay()
{
    if (m_session && FinancialUIMap::userLabel)
        FinancialUIMap::userLabel->setText(
            QString("%1  |  %2")
                .arg(m_session->displayName())
                .arg(m_session->role()));
}

// ============================================================================
// TAB 1 — OVERVIEW
// ============================================================================

void FinancialWindow::onMonthYearChanged()
{
    loadMonthlySummary();
    loadWeeklyTable();
    loadTopSalesTable();
    loadOneTimeExpenses();
}

void FinancialWindow::loadMonthlySummary()
{
    if (!m_db) return;

    const int month = FinancialUIMap::monthSelect->currentData().toInt();
    const int year  = FinancialUIMap::yearSelect->currentData().toInt();
    const QString mm = QString("%1").arg(month, 2, 10, QChar('0'));
    const QString yyyy = QString::number(year);

    // ── Income: sum of completed ticket totals ────────────────────────────────
    QSqlQuery incQ(m_db->database());
    incQ.prepare(
        "SELECT COALESCE(SUM(total), 0) FROM tickets "
        "WHERE status = 'COMPLETED' "
        "AND strftime('%Y', date) = ? AND strftime('%m', date) = ?");
    incQ.addBindValue(yyyy);
    incQ.addBindValue(mm);
    double income = 0.0;
    if (incQ.exec() && incQ.next()) income = incQ.value(0).toDouble();

    // ── Cost: sum of unit_cost * quantity for completed tickets ───────────────
    QSqlQuery costQ(m_db->database());
    costQ.prepare(
        "SELECT COALESCE(SUM(ti.unit_cost * ti.quantity), 0) "
        "FROM ticket_items ti "
        "JOIN tickets t ON ti.ticket_id = t.id "
        "WHERE t.status = 'COMPLETED' "
        "AND strftime('%Y', t.date) = ? AND strftime('%m', t.date) = ?");
    costQ.addBindValue(yyyy);
    costQ.addBindValue(mm);
    double cost = 0.0;
    if (costQ.exec() && costQ.next()) cost = costQ.value(0).toDouble();

    // ── Expenses: one-time + recurring payments this month ────────────────────
    QSqlQuery otQ(m_db->database());
    otQ.prepare(Queries::ONE_TIME_EXPENSE_SELECT_BY_MONTH);
    otQ.addBindValue(yyyy);
    otQ.addBindValue(mm);
    double otExpenses = 0.0;
    if (otQ.exec())
        while (otQ.next()) otExpenses += otQ.value("amount").toDouble();

    QSqlQuery recQ(m_db->database());
    recQ.prepare(Queries::REC_EXPENSE_PAYMENT_SELECT_BY_MONTH);
    recQ.addBindValue(yyyy);
    recQ.addBindValue(mm);
    double recExpenses = 0.0;
    if (recQ.exec())
        while (recQ.next()) recExpenses += recQ.value("amount").toDouble();

    const double expenses = otExpenses + recExpenses;
    const double benefit  = income - cost;
    const double rest     = benefit - expenses;

    // ── Update table ──────────────────────────────────────────────────────────
    auto setCell = [&](int col, double val) {
        QTableWidgetItem* item = FinancialUIMap::monthlyTable->item(0, col);
        if (!item) {
            item = new QTableWidgetItem();
            item->setTextAlignment(Qt::AlignCenter);
            FinancialUIMap::monthlyTable->setItem(0, col, item);
        }
        item->setText(AppHelpers::formatPrice(val));
    };

    setCell(FinancialConstants::COL_MONTHLY_INCOME,   income);
    setCell(FinancialConstants::COL_MONTHLY_COST,     cost);
    setCell(FinancialConstants::COL_MONTHLY_BENEFIT,  benefit);
    setCell(FinancialConstants::COL_MONTHLY_EXPENSES, expenses);
    setCell(FinancialConstants::COL_MONTHLY_REST,     rest);
}

void FinancialWindow::loadWeeklyTable()
{
    if (!m_db) return;

    const QDate today    = QDate::currentDate();
    // Qt: Monday=1, Sunday=7. Week starts Sunday.
    const QDate sunday = today.addDays(-(today.dayOfWeek() % 7));
    const QDate saturday = sunday.addDays(6);

    double totalIncome   = 0.0;
    double totalExpenses = 0.0;

    for (int d = 0; d < 7; ++d) {
        const QDate day = sunday.addDays(d);
        const QString dateStr = day.toString("yyyy-MM-dd");

        // Income for this day
        QSqlQuery incQ(m_db->database());
        incQ.prepare(
            "SELECT COALESCE(SUM(total), 0) FROM tickets "
            "WHERE status = 'COMPLETED' AND date = ?");
        incQ.addBindValue(dateStr);
        double dayIncome = 0.0;
        if (incQ.exec() && incQ.next()) dayIncome = incQ.value(0).toDouble();

        // One-time expenses for this day
        QSqlQuery otQ(m_db->database());
        otQ.prepare("SELECT COALESCE(SUM(amount), 0) FROM one_time_expenses WHERE date = ?");
        otQ.addBindValue(dateStr);
        double dayExpenses = 0.0;
        if (otQ.exec() && otQ.next()) dayExpenses = otQ.value(0).toDouble();

        // Recurring expense payments for this day
        QSqlQuery recQ(m_db->database());
        recQ.prepare(
            "SELECT COALESCE(SUM(amount), 0) FROM recurring_expense_payments WHERE paid_date = ?");
        recQ.addBindValue(dateStr);
        if (recQ.exec() && recQ.next()) dayExpenses += recQ.value(0).toDouble();

        totalIncome   += dayIncome;
        totalExpenses += dayExpenses;

        const int row = d;
        auto setVal = [&](int col, const QString& text) {
            QTableWidgetItem* item = FinancialUIMap::weeklyTable->item(row, col);
            if (!item) {
                item = new QTableWidgetItem();
                item->setTextAlignment(Qt::AlignCenter);
                FinancialUIMap::weeklyTable->setItem(row, col, item);
            }
            item->setText(text);
        };

        setVal(FinancialConstants::COL_WEEKLY_INCOME,   AppHelpers::formatPrice(dayIncome));
        setVal(FinancialConstants::COL_WEEKLY_EXPENSES, AppHelpers::formatPrice(dayExpenses));
    }

    // Total row (row 7)
    auto setTotal = [&](int col, const QString& text) {
        QTableWidgetItem* item = FinancialUIMap::weeklyTable->item(7, col);
        if (!item) {
            item = new QTableWidgetItem();
            item->setTextAlignment(Qt::AlignCenter);
            FinancialUIMap::weeklyTable->setItem(7, col, item);
        }
        item->setText(text);
    };
    setTotal(FinancialConstants::COL_WEEKLY_INCOME,   AppHelpers::formatPrice(totalIncome));
    setTotal(FinancialConstants::COL_WEEKLY_EXPENSES, AppHelpers::formatPrice(totalExpenses));
}

void FinancialWindow::loadTopSalesTable()
{
    if (!m_db) return;

    const int month = FinancialUIMap::monthSelect->currentData().toInt();
    const int year  = FinancialUIMap::yearSelect->currentData().toInt();
    const QString mm   = QString("%1").arg(month, 2, 10, QChar('0'));
    const QString yyyy = QString::number(year);

    QSqlQuery q(m_db->database());
    q.prepare(
        "SELECT ti.product_name, SUM(ti.quantity) as total_qty "
        "FROM ticket_items ti "
        "JOIN tickets t ON ti.ticket_id = t.id "
        "WHERE t.status = 'COMPLETED' "
        "AND strftime('%Y', t.date) = ? AND strftime('%m', t.date) = ? "
        "GROUP BY ti.product_name "
        "ORDER BY total_qty DESC "
        "LIMIT 3");
    q.addBindValue(yyyy);
    q.addBindValue(mm);

    if (!q.exec()) {
        qWarning() << "Failed to load top sales:" << q.lastError().text();
        return;
    }

    QList<QPair<QString, double>> topItems;
    while (q.next()) {
        topItems.append(qMakePair(
            q.value("product_name").toString(),
            q.value("total_qty").toDouble()
        ));
    }

    for (int row = 0; row < 3; ++row) {
        auto setVal = [&](int col, const QString& text) {
            QTableWidgetItem* item = FinancialUIMap::topSalesTable->item(row, col);
            if (!item) {
                item = new QTableWidgetItem();
                item->setTextAlignment(Qt::AlignCenter);
                FinancialUIMap::topSalesTable->setItem(row, col, item);
            }
            item->setText(text);
        };

        if (row < topItems.size()) {
            setVal(FinancialConstants::COL_TOPSALES_PRODUCT, topItems[row].first);
            setVal(FinancialConstants::COL_TOPSALES_QTY,     QString::number(topItems[row].second, 'f', 0));
        } else {
            setVal(FinancialConstants::COL_TOPSALES_PRODUCT, "—");
            setVal(FinancialConstants::COL_TOPSALES_QTY,     "—");
        }
    }
}

void FinancialWindow::loadOneTimeExpenses()
{
    if (!m_db) return;

    const int month = FinancialUIMap::monthSelect->currentData().toInt();
    const int year  = FinancialUIMap::yearSelect->currentData().toInt();
    const QString mm   = QString("%1").arg(month, 2, 10, QChar('0'));
    const QString yyyy = QString::number(year);

    FinancialUIMap::expensesTable->setRowCount(0);

    QSqlQuery query(m_db->database());
    query.prepare(Queries::ONE_TIME_EXPENSE_SELECT_BY_MONTH);
    query.addBindValue(yyyy);
    query.addBindValue(mm);

    if (!query.exec()) {
        qWarning() << "loadOneTimeExpenses:" << query.lastError().text();
        return;
    }

    while (query.next()) {
        const int     id     = query.value("id").toInt();
        const QString name   = query.value("name").toString();
        const double  amount = query.value("amount").toDouble();
        const QString date   = query.value("date").toString();

        const int row = FinancialUIMap::expensesTable->rowCount();
        FinancialUIMap::expensesTable->insertRow(row);

        auto makeItem = [&](const QString& text) {
            QTableWidgetItem* item = new QTableWidgetItem(text);
            item->setTextAlignment(Qt::AlignCenter);
            item->setData(Qt::UserRole, id);
            return item;
        };

        FinancialUIMap::expensesTable->setItem(row, FinancialConstants::COL_EXP_NAME,
                                               makeItem(name));
        FinancialUIMap::expensesTable->setItem(row, FinancialConstants::COL_EXP_AMOUNT,
                                               makeItem(AppHelpers::formatPrice(amount)));
        FinancialUIMap::expensesTable->setItem(row, FinancialConstants::COL_EXP_DATE,
                                               makeItem(AppHelpers::formatDate(QDate::fromString(date, "yyyy-MM-dd"))));
    }
}

void FinancialWindow::onAddExpenseClicked()
{
    // Single dialog for name + amount
    QDialog dlg(this);
    dlg.setWindowTitle("Add Expense");
    dlg.setFixedWidth(320);

    QVBoxLayout* layout = new QVBoxLayout(&dlg);
    layout->setSpacing(10);
    layout->setContentsMargins(16, 16, 16, 16);

    QLineEdit* nameEdit = new QLineEdit(&dlg);
    nameEdit->setPlaceholderText("Expense name");

    QDoubleSpinBox* amountSpin = new QDoubleSpinBox(&dlg);
    amountSpin->setMinimum(0.001);
    amountSpin->setMaximum(999999.0);
    amountSpin->setDecimals(3);
    amountSpin->setSuffix(QString(" %1").arg(AppConstants::CURRENCY));

    QDialogButtonBox* btns = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    layout->addWidget(new QLabel("Name:", &dlg));
    layout->addWidget(nameEdit);
    layout->addWidget(new QLabel("Amount:", &dlg));
    layout->addWidget(amountSpin);
    layout->addWidget(btns);

    if (dlg.exec() != QDialog::Accepted) return;
    const QString name   = nameEdit->text().trimmed();
    const double  amount = amountSpin->value();
    if (name.isEmpty()) {
        AppHelpers::showWarning(this, "Please enter an expense name.");
        return;
    }

    QSqlQuery ins(m_db->database());
    ins.prepare(Queries::ONE_TIME_EXPENSE_INSERT);
    ins.addBindValue(name.trimmed());
    ins.addBindValue(amount);
    ins.addBindValue(QDate::currentDate().toString("yyyy-MM-dd"));
    ins.addBindValue(QVariant()); // notes

    if (!ins.exec()) {
        AppHelpers::showError(this, "Failed to add expense.\n\n" + ins.lastError().text());
        return;
    }

    loadOneTimeExpenses();
    loadMonthlySummary();
}

void FinancialWindow::onDeleteExpenseClicked()
{
    const int id = selectedOneTimeExpenseId();
    if (id <= 0) {
        AppHelpers::showWarning(this, FinancialConstants::MSG_NO_SELECTION);
        return;
    }

    if (!AppHelpers::confirmAction(this, FinancialConstants::MSG_DELETE_CONFIRM)) return;

    QSqlQuery del(m_db->database());
    del.prepare(Queries::ONE_TIME_EXPENSE_DELETE);
    del.addBindValue(id);

    if (!del.exec()) {
        AppHelpers::showError(this, "Failed to delete expense.\n\n" + del.lastError().text());
        return;
    }

    loadOneTimeExpenses();
    loadMonthlySummary();
}

int FinancialWindow::selectedOneTimeExpenseId() const
{
    const QList<QTableWidgetItem*> selected =
        FinancialUIMap::expensesTable->selectedItems();
    if (selected.isEmpty()) return -1;
    return selected.first()->data(Qt::UserRole).toInt();
}

// ============================================================================
// TAB 2 — YEARLY SUMMARY
// ============================================================================

void FinancialWindow::onYearlyTableDoubleClicked(int row, int column)
{
    Q_UNUSED(column)
    if (row < 0 || row >= 12) return;

    const int year  = QDate::currentDate().year();
    const int month = row + 1;

    const QString mm   = QString("%1").arg(month, 2, 10, QChar('0'));
    const QString yyyy = QString::number(year);

    QSqlQuery incQ(m_db->database());
    incQ.prepare(
        "SELECT COALESCE(SUM(total), 0), COUNT(*) FROM tickets "
        "WHERE status = 'COMPLETED' "
        "AND strftime('%Y', date) = ? AND strftime('%m', date) = ?");
    incQ.addBindValue(yyyy);
    incQ.addBindValue(mm);

    double income = 0.0;
    int    count  = 0;
    if (incQ.exec() && incQ.next()) {
        income = incQ.value(0).toDouble();
        count  = incQ.value(1).toInt();
    }

    const QStringList monthNames = {
        "January","February","March","April","May","June",
        "July","August","September","October","November","December"
    };

    AppHelpers::showInfo(this,
                         QString("%1 %2\n\nTransactions: %3\nTotal Income: %4")
                             .arg(monthNames[month - 1])
                             .arg(year)
                             .arg(count)
                             .arg(AppHelpers::formatPrice(income)));
}

void FinancialWindow::loadYearlyTable()
{
    if (!m_db) return;

    const int currentYear = QDate::currentDate().year();
    const QString yyyy = QString::number(currentYear);

    double totalIncome    = 0.0;
    double totalCost     = 0.0;
    double totalExpenses = 0.0;
    double totalBenefit  = 0.0;

    for (int m = 1; m <= 12; ++m) {
        const QString mm = QString("%1").arg(m, 2, 10, QChar('0'));

        // Total Income (from completed tickets)
        QSqlQuery incQ(m_db->database());
        incQ.prepare(
            "SELECT COALESCE(SUM(total), 0) FROM tickets "
            "WHERE status = 'COMPLETED' "
            "AND strftime('%Y', date) = ? AND strftime('%m', date) = ?");
        incQ.addBindValue(yyyy);
        incQ.addBindValue(mm);
        double income = 0.0;
        if (incQ.exec() && incQ.next()) income = incQ.value(0).toDouble();

        // Total Cost (unit_cost * quantity from ticket_items for completed tickets)
        QSqlQuery costQ(m_db->database());
        costQ.prepare(
            "SELECT COALESCE(SUM(ti.unit_cost * ti.quantity), 0) "
            "FROM ticket_items ti "
            "JOIN tickets t ON ti.ticket_id = t.id "
            "WHERE t.status = 'COMPLETED' "
            "AND strftime('%Y', t.date) = ? AND strftime('%m', t.date) = ?");
        costQ.addBindValue(yyyy);
        costQ.addBindValue(mm);
        double cost = 0.0;
        if (costQ.exec() && costQ.next()) cost = costQ.value(0).toDouble();

        // Total Expenses (one-time expenses for this month)
        QSqlQuery expQ(m_db->database());
        expQ.prepare(
            "SELECT COALESCE(SUM(amount), 0) FROM one_time_expenses "
            "WHERE strftime('%Y', expense_date) = ? AND strftime('%m', expense_date) = ?");
        expQ.addBindValue(yyyy);
        expQ.addBindValue(mm);
        double expenses = 0.0;
        if (expQ.exec() && expQ.next()) expenses = expQ.value(0).toDouble();

        // Total Benefit = Income - Cost - Expenses
        const double benefit = income - cost - expenses;

        totalIncome    += income;
        totalCost     += cost;
        totalExpenses += expenses;
        totalBenefit  += benefit;

        const int row = m - 1;
        auto setCell = [&](int col, const QString& text) {
            QTableWidgetItem* item = FinancialUIMap::yearlyTable->item(row, col);
            if (!item) {
                item = new QTableWidgetItem();
                item->setTextAlignment(Qt::AlignCenter);
                FinancialUIMap::yearlyTable->setItem(row, col, item);
            }
            item->setText(text);
            item->setData(Qt::UserRole, m);
        };

        setCell(FinancialConstants::COL_YEAR_INCOME,    AppHelpers::formatPrice(income));
        setCell(FinancialConstants::COL_YEAR_COST,     AppHelpers::formatPrice(cost));
        setCell(FinancialConstants::COL_YEAR_EXPENSES, AppHelpers::formatPrice(expenses));
        setCell(FinancialConstants::COL_YEAR_BENEFIT,  AppHelpers::formatPrice(benefit));

        if (benefit < 0) {
            QTableWidgetItem* item = FinancialUIMap::yearlyTable->item(row, FinancialConstants::COL_YEAR_BENEFIT);
            if (item) item->setForeground(Qt::red);
        } else {
            QTableWidgetItem* item = FinancialUIMap::yearlyTable->item(row, FinancialConstants::COL_YEAR_BENEFIT);
            if (item) item->setForeground(QColor("#2E7D32"));
        }
    }

    // Total row (row 12)
    auto setTotal = [&](int col, const QString& text) {
        QTableWidgetItem* item = FinancialUIMap::yearlyTable->item(12, col);
        if (!item) {
            item = new QTableWidgetItem();
            item->setTextAlignment(Qt::AlignCenter);
            QFont boldFont = item->font();
            boldFont.setBold(true);
            boldFont.setPointSize(boldFont.pointSize() + 2);
            item->setFont(boldFont);
            FinancialUIMap::yearlyTable->setItem(12, col, item);
        }
        item->setText(text);
    };
    setTotal(FinancialConstants::COL_YEAR_INCOME,    AppHelpers::formatPrice(totalIncome));
    setTotal(FinancialConstants::COL_YEAR_COST,     AppHelpers::formatPrice(totalCost));
    setTotal(FinancialConstants::COL_YEAR_EXPENSES, AppHelpers::formatPrice(totalExpenses));
    setTotal(FinancialConstants::COL_YEAR_BENEFIT,  AppHelpers::formatPrice(totalBenefit));

    if (totalBenefit < 0) {
        QTableWidgetItem* item = FinancialUIMap::yearlyTable->item(12, FinancialConstants::COL_YEAR_BENEFIT);
        if (item) item->setForeground(Qt::red);
    } else {
        QTableWidgetItem* item = FinancialUIMap::yearlyTable->item(12, FinancialConstants::COL_YEAR_BENEFIT);
        if (item) item->setForeground(QColor("#2E7D32"));
    }
}

// ============================================================================
// LOGOUT
// ============================================================================
// LOGOUT
// ============================================================================

void FinancialWindow::onLogoutClicked()
{
    if (!LogoutManager::confirmAndLogout(this, m_session->sessionId(), m_session->username())) {
        return;
    }
    close();
    emit logoutRequested();
}
