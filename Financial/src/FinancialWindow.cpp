#include "FinancialWindow.h"
#include "Animations.h"
#include "FinancialWidgets.h"
#include "LineIcons.h"
#include "ModernDialog.h"
#include "ui_financialwindow.h"
#include "FinancialUIMap.h"
#include "FinancialConstants.h"
#include "LocalApi.h"
#include "SessionManager.h"
#include "StyleManager.h"
#include "UserSession.h"
#include "CommonConfig.h"
#include "Prefixes.h"
#include "NotificationManager.h"
#include "NotificationBell.h"
#include "NotificationActions.h"
#include "ChangeNotifier.h"
#include "LogoutManager.h"
#include "TranslationManager.h"
#include <QBrush>
#include <QComboBox>
#include <QDate>
#include <QDebug>
#include <QFrame>
#include <QGroupBox>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStyle>
#include <QTableWidget>
#include <QTableWidgetItem>

FinancialWindow::FinancialWindow(UserSession* session,
                                 LocalApi* api,
                                 ChangeNotifier* notifier,
                                 QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::FinancialWindow)
    , m_session(session)
    , m_api(api)
    , m_notifier(notifier)
{
    ui->setupUi(this);

    FinancialUIMap::initialize(ui);
    buildLayout();

    m_bell = new NotificationBell(m_api, FinancialUIMap::notificationBtn,
                                  m_session ? m_session->username() : QString(),
                                  m_notifier, this);

    retranslateUi();
    setupConnections();
    updateUserDisplay();

    // Load all data
    loadMonthlySummary();
    loadWeeklyTable();
    loadTopSalesTable();
    loadOneTimeExpenses();
    loadYearlyTable();

    connect(FinancialUIMap::yearlyTable, &QTableWidget::currentCellChanged, this, [this](int row) {
        m_yearChart->setSelectedMonth(row >= 0 && row < 12 ? row + 1 : 0);
    });
    connect(m_yearChart, &YearChart::monthClicked, this, [](int month) {
        FinancialUIMap::yearlyTable->setCurrentCell(month - 1, 0);
    });
    FinancialUIMap::yearlyTable->setCurrentCell(QDate::currentDate().month() - 1, 0);

    connect(&TranslationManager::instance(), &TranslationManager::languageChanged,
            this, [this]() {
                retranslateUi();
                loadMonthlySummary();
                loadWeeklyTable();
                loadYearlyTable();
            });

    showMaximized();
}

FinancialWindow::~FinancialWindow()
{
    delete ui;
}

void FinancialWindow::retranslateUi()
{
    setWindowTitle(tr("POSBox - Financial"));
    ui->tabwidget->setTabText(0, tr("Overview"));
    ui->tabwidget->setTabText(1, tr("Yearly Summary"));
    FinancialUIMap::notificationBtn->setToolTip(tr("Notifications"));
    FinancialUIMap::logoutBtn->setToolTip(tr("Logout"));

    // Month names in the app language.
    const QLocale locale = TranslationManager::instance().locale();
    {
        QSignalBlocker block(FinancialUIMap::monthSelect);
        for (int i = 0; i < 12 && i < FinancialUIMap::monthSelect->count(); ++i)
            FinancialUIMap::monthSelect->setItemText(i, locale.standaloneMonthName(i + 1, QLocale::LongFormat));
    }
    for (int row = 0; row < 12; ++row) {
        if (QTableWidgetItem* item = FinancialUIMap::yearlyTable->item(row, FinancialConstants::COL_YEAR_MONTH))
            item->setText(locale.standaloneMonthName(row + 1, QLocale::LongFormat));
    }
    if (QTableWidgetItem* total = FinancialUIMap::yearlyTable->item(12, FinancialConstants::COL_YEAR_MONTH))
        total->setText(tr("Total"));

    const QString currency = QString::fromUtf8(AppConstants::CURRENCY);
    m_kpiIncome.title->setText(tr("Income"));
    m_kpiIncome.sub->setText(tr("Everything sold, in %1").arg(currency));
    m_kpiCost.title->setText(tr("Cost"));
    m_kpiCost.sub->setText(tr("What those items cost"));
    m_kpiBenefit.title->setText(tr("Benefit"));
    m_kpiBenefit.sub->setText(tr("Income − Cost"));
    m_kpiExpenses.title->setText(tr("Expenses"));
    m_kpiExpenses.sub->setText(tr("Expenses and withdrawals"));
    m_kpiRest.title->setText(tr("Rest"));
    m_kpiRest.sub->setText(tr("Benefit − Expenses"));

    m_weekLegend->setText(QString("<span style='color:#27AE60'>■</span> %1 &nbsp; "
                                  "<span style='color:#E74C3C'>■</span> %2")
                              .arg(tr("Income"), tr("Expenses")));
    m_topTitle->setText(tr("Top sellers this month"));
    m_expenseTitle->setText(tr("One-time expenses"));
    m_expenseTotalCaption->setText(tr("Total"));
    FinancialUIMap::addExpenseBtn->setText(tr("Add"));
    FinancialUIMap::deleteExpenseBtn->setText(tr("Delete"));
    FinancialUIMap::expensesTable->horizontalHeaderItem(FinancialConstants::COL_EXP_NAME)->setText(tr("Name"));
    FinancialUIMap::expensesTable->horizontalHeaderItem(FinancialConstants::COL_EXP_AMOUNT)->setText(tr("Amount"));

    m_yearCaption->setText(tr("Yearly Summary"));
    m_chartTitle->setText(tr("Income by month"));
    m_yearChart->setToolTip(tr("Click a month to select it"));
    QTableWidget* yearly = FinancialUIMap::yearlyTable;
    yearly->horizontalHeaderItem(FinancialConstants::COL_YEAR_MONTH)->setText(tr("Month (amounts in %1)").arg(currency));
    yearly->horizontalHeaderItem(FinancialConstants::COL_YEAR_INCOME)->setText(tr("Income"));
    yearly->horizontalHeaderItem(FinancialConstants::COL_YEAR_COST)->setText(tr("Cost"));
    yearly->horizontalHeaderItem(FinancialConstants::COL_YEAR_EXPENSES)->setText(tr("Expenses"));
    yearly->horizontalHeaderItem(FinancialConstants::COL_YEAR_BENEFIT)->setText(tr("Benefit"));
}

void FinancialWindow::handleNavigation(const QString& action)
{
    Q_UNUSED(action)
    // "Financial|show" (and anything else) is a focus request.
    AppHelpers::bringToFront(this);
}

// ============================================================================
// SETUP
// ============================================================================

void FinancialWindow::setupConnections()
{
    // Top bar
    connect(FinancialUIMap::notificationBtn, &QPushButton::clicked, m_bell, &NotificationBell::togglePanel);
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
    connect(FinancialUIMap::yearlySelect, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FinancialWindow::onYearlySelectChanged);
    connect(FinancialUIMap::yearlyTable, &QTableWidget::cellDoubleClicked,
            this, &FinancialWindow::onYearlyTableDoubleClicked);

    if (m_notifier) {
        connect(m_notifier, &ChangeNotifier::appNavigateRequested,
                this, &FinancialWindow::handleNavigation);
        // A sale, refund or withdrawal in another app changes the numbers here.
        auto refresh = [this](int) {
            loadMonthlySummary();
            loadWeeklyTable();
            loadTopSalesTable();
            loadOneTimeExpenses();
            loadYearlyTable();
        };
        connect(m_notifier, &ChangeNotifier::ticketCreated, this, refresh);
        connect(m_notifier, &ChangeNotifier::ticketUpdated, this, refresh);
        connect(m_notifier, &ChangeNotifier::expenseUpdated, this, refresh);
    }
}

void FinancialWindow::updateUserDisplay()
{
    if (m_session && FinancialUIMap::userLabel)
        FinancialUIMap::userLabel->setText(
            QString("%1  |  %2")
                .arg(m_session->displayName())
                .arg(AppHelpers::roleName(m_session->role())));
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
    if (!m_api) return;

    const int month = FinancialUIMap::monthSelect->currentData().toInt();
    const int year  = FinancialUIMap::yearSelect->currentData().toInt();
    const QString mm = QString("%1").arg(month, 2, 10, QChar('0'));
    const QString yyyy = QString::number(year);

    // ── Income: sum of completed ticket totals ────────────────────────────────
    const double income = m_api->incomeByMonth(yyyy, mm);

    // ── Cost: sum of unit_cost * quantity for completed tickets ───────────────
    const double cost = m_api->ticketCostByMonth(yyyy, mm);

    // ── Expenses: one-time expenses and register withdrawals ──────────────────
    double expenses = 0.0;
    for (const DataRow& r : m_api->expensesByMonth(yyyy, mm))
        expenses += r.value("amount").toDouble();

    const double benefit  = income - cost;
    const double rest     = benefit - expenses;

    setKpiValue(m_kpiIncome, income);
    setKpiValue(m_kpiCost, cost);
    setKpiValue(m_kpiBenefit, benefit);
    setKpiValue(m_kpiExpenses, expenses);
    setKpiValue(m_kpiRest, rest);

    // Rest is the number that matters most: red when the month is short.
    const char* restTone = rest < 0 ? "bad" : "good";
    for (QWidget* widget : QList<QWidget*>{m_kpiRest.card, m_kpiRest.bar, m_kpiRest.value}) {
        widget->setProperty("tone", restTone);
        widget->style()->unpolish(widget);
        widget->style()->polish(widget);
    }
    m_kpiRest.card->setProperty("hero", true);
    m_kpiRest.card->style()->unpolish(m_kpiRest.card);
    m_kpiRest.card->style()->polish(m_kpiRest.card);

    m_periodTitle->setText(QString("%1 %2").arg(FinancialUIMap::monthSelect->currentText(), yyyy));
}

void FinancialWindow::setKpiValue(Kpi& kpi, double value)
{
    QLabel* label = kpi.value;
    Animations::animateValues(label, {value}, [label](const QVector<double>& shown) {
        label->setText(AppHelpers::formatAmount(shown[0]));
    });
}

void FinancialWindow::loadWeeklyTable()
{
    if (!m_api) return;

    const QDate today    = QDate::currentDate();
    // Qt: Monday=1, Sunday=7. Week starts Sunday.
    const QDate sunday = today.addDays(-(today.dayOfWeek() % 7));
    const QDate saturday = sunday.addDays(6);

    // Expenses per day of the week, from the one or two calendar months it spans.
    QHash<QString, double> expensesByDay;
    QList<QDate> months{sunday};
    if (saturday.month() != sunday.month())
        months << saturday;
    for (const QDate& month : months) {
        for (const DataRow& r : m_api->expensesByMonth(month.toString("yyyy"), month.toString("MM")))
            expensesByDay[r.value("date").toString()] += r.value("amount").toDouble();
    }

    QVector<double> incomeByDay(7);
    QVector<double> expenseByDay(7);
    for (int d = 0; d < 7; ++d) {
        const QString dateStr = sunday.addDays(d).toString("yyyy-MM-dd");
        incomeByDay[d] = m_api->dailyIncome(dateStr);
        expenseByDay[d] = expensesByDay.value(dateStr);
    }
    m_weekChart->setDays(incomeByDay, expenseByDay, sunday.daysTo(today));

    // Always the current week, whichever month is selected above: say so.
    const QLocale locale = TranslationManager::instance().locale();
    m_weekTitle->setText(tr("This week  ·  %1").arg(
        sunday.month() == saturday.month()
            ? QString("%1–%2").arg(sunday.day()).arg(locale.toString(saturday, "d MMM"))
            : QString("%1 – %2").arg(locale.toString(sunday, "d MMM"), locale.toString(saturday, "d MMM"))));
}

void FinancialWindow::loadTopSalesTable()
{
    if (!m_api) return;

    const int month = FinancialUIMap::monthSelect->currentData().toInt();
    const int year  = FinancialUIMap::yearSelect->currentData().toInt();
    const QString mm   = QString("%1").arg(month, 2, 10, QChar('0'));
    const QString yyyy = QString::number(year);

    QList<QPair<QString, double>> topItems;
    for (const DataRow& r : m_api->topProductsByMonth(yyyy, mm)) {
        topItems.append(qMakePair(r.value("product_name").toString(),
                                  r.value("total_qty").toDouble()));
    }

    m_topSellers->setItems(topItems);
}

void FinancialWindow::loadOneTimeExpenses()
{
    if (!m_api) return;

    const int month = FinancialUIMap::monthSelect->currentData().toInt();
    const int year  = FinancialUIMap::yearSelect->currentData().toInt();
    const QString mm   = QString("%1").arg(month, 2, 10, QChar('0'));
    const QString yyyy = QString::number(year);

    FinancialUIMap::expensesTable->setRowCount(0);

    double total = 0.0;
    const QList<DataRow> rows = m_api->expensesByMonth(yyyy, mm);
    for (const DataRow& r : rows) {
        const int     id     = r.value("id").toInt();
        const QString name   = r.value("name").toString();
        const double  amount = r.value("amount").toDouble();
        const QString date   = r.value("date").toString();
        total += amount;

        const int row = FinancialUIMap::expensesTable->rowCount();
        FinancialUIMap::expensesTable->insertRow(row);

        auto* nameItem = new QTableWidgetItem(name);
        nameItem->setData(Qt::UserRole, id);
        nameItem->setData(TwoLineDelegate::SecondLineRole,
                          AppHelpers::formatDate(QDate::fromString(date, "yyyy-MM-dd")));
        nameItem->setToolTip(name);
        FinancialUIMap::expensesTable->setItem(row, FinancialConstants::COL_EXP_NAME, nameItem);

        auto* amountItem = new QTableWidgetItem(AppHelpers::formatAmount(amount));
        amountItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        amountItem->setData(Qt::UserRole, id);
        FinancialUIMap::expensesTable->setItem(row, FinancialConstants::COL_EXP_AMOUNT, amountItem);
    }
    m_expenseTotal->setText(AppHelpers::formatAmount(total));
}

void FinancialWindow::onAddExpenseClicked()
{
    // Name and amount; the date is always today.
    ModernDialog dlg(ModernDialog::Look::Band, "receipt", ModernDialog::Tone::Red, tr("Add expense"),
                     tr("Dated today  ·  %1").arg(AppHelpers::formatDate(QDate::currentDate())),
                     420, this);
    auto* nameEdit = new QLineEdit(&dlg);
    nameEdit->setPlaceholderText(tr("What was it for?"));
    dlg.addRow(tr("Name"), nameEdit);
    QLineEdit* amountEdit = ModernDialog::moneyEdit(&dlg, true);
    amountEdit->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    dlg.addRow(tr("Amount"), amountEdit);

    QPushButton* addBtn = dlg.addFooter(tr("Add Expense"), "plus", "add");
    connect(addBtn, &QPushButton::clicked, &dlg, [&]() {
        if (nameEdit->text().trimmed().isEmpty()) {
            AppHelpers::showWarning(&dlg, tr("Please enter an expense name."));
            nameEdit->setFocus();
        } else if (ModernDialog::moneyValue(amountEdit) <= 0) {
            AppHelpers::showWarning(&dlg, tr("Please enter an amount."));
            amountEdit->setFocus();
        } else {
            dlg.accept();
        }
    });
    nameEdit->setFocus();

    if (dlg.exec() != QDialog::Accepted) return;
    const QString name   = nameEdit->text().trimmed();
    const double  amount = ModernDialog::moneyValue(amountEdit);

    if (m_api->insert("one_time_expenses",
                      {{"name", name},
                       {"amount", amount},
                       {"date", QDate::currentDate().toString("yyyy-MM-dd")},
                       {"notes", QVariant()}}) < 0) {
        AppHelpers::showError(this, tr("Failed to add expense.") + "\n\n" + m_api->lastError());
        return;
    }
    if (m_notifier)
        m_notifier->notifyChange(ChangeType::EXPENSE_UPDATED, 0);

    loadOneTimeExpenses();
    loadMonthlySummary();
    loadWeeklyTable();
    loadYearlyTable();
}

void FinancialWindow::onDeleteExpenseClicked()
{
    const int id = selectedOneTimeExpenseId();
    if (id <= 0) {
        AppHelpers::showWarning(this, AppHelpers::trMsg(FinancialConstants::MSG_NO_SELECTION));
        return;
    }

    if (!AppHelpers::confirmAction(this, AppHelpers::trMsg(FinancialConstants::MSG_DELETE_CONFIRM))) return;

    if (!m_api->remove("one_time_expenses", {{"id", id}})) {
        AppHelpers::showError(this, tr("Failed to delete expense.") + "\n\n" + m_api->lastError());
        return;
    }
    if (m_notifier)
        m_notifier->notifyChange(ChangeType::EXPENSE_UPDATED, id);

    loadOneTimeExpenses();
    loadMonthlySummary();
    loadWeeklyTable();
    loadYearlyTable();
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

void FinancialWindow::loadYearlyTable()
{
    if (!m_api) return;

    const int year  = FinancialUIMap::yearlySelect->currentData().toInt();
    const QString yyyy = QString::number(year);

    // Monthly income for the whole year in one query.
    QHash<QString, double> incomeByMonth;
    for (const DataRow& r : m_api->incomeByYear(yyyy))
        incomeByMonth.insert(r.value("month").toString(), r.value("total").toDouble());

    // Months still to come are shown quieter and left out of the totals. A past
    // year counts all 12 months; a future year has nothing to count yet.
    const QDate today = QDate::currentDate();
    const int lastMonth = year < today.year() ? 12 : year > today.year() ? 0 : today.month();
    QVector<double> monthlyIncome(12);

    double totalIncome   = 0.0;
    double totalCost     = 0.0;
    double totalExpenses = 0.0;
    double totalBenefit  = 0.0;

    for (int m = 1; m <= 12; ++m) {
        const QString mm = QString("%1").arg(m, 2, 10, QChar('0'));

        const double income = incomeByMonth.value(mm, 0.0);
        monthlyIncome[m - 1] = income;
        const bool counted = m <= lastMonth;
        const double cost = counted ? m_api->ticketCostByMonth(yyyy, mm) : 0.0;
        double expenses = 0.0;
        if (counted) {
            for (const DataRow& r : m_api->expensesByMonth(yyyy, mm))
                expenses += r.value("amount").toDouble();
        }
        const double benefit = income - cost - expenses;

        if (counted) {
            totalIncome   += income;
            totalCost     += cost;
            totalExpenses += expenses;
            totalBenefit  += benefit;
        }

        const int row = m - 1;
        const QBrush rowColor = m > lastMonth
            ? QBrush(LineIcons::colors().muted) : QBrush();
        auto setCell = [&](int col, const QString& text) {
            QTableWidgetItem* item = FinancialUIMap::yearlyTable->item(row, col);
            item->setText(text);
            item->setData(Qt::UserRole, m); // month number for double-click
            item->setForeground(rowColor);
        };

        FinancialUIMap::yearlyTable->item(row, FinancialConstants::COL_YEAR_MONTH)->setForeground(rowColor);
        setCell(FinancialConstants::COL_YEAR_INCOME,   AppHelpers::formatAmount(income));
        setCell(FinancialConstants::COL_YEAR_COST,     counted ? AppHelpers::formatAmount(cost) : QStringLiteral("—"));
        setCell(FinancialConstants::COL_YEAR_EXPENSES, counted ? AppHelpers::formatAmount(expenses) : QStringLiteral("—"));
        setCell(FinancialConstants::COL_YEAR_BENEFIT,  counted ? AppHelpers::formatAmount(benefit) : QStringLiteral("—"));
    }

    // Total row (row 12)
    auto setTotal = [&](int col, const QString& text) {
        FinancialUIMap::yearlyTable->item(12, col)->setText(text);
    };
    setTotal(FinancialConstants::COL_YEAR_INCOME,   AppHelpers::formatAmount(totalIncome));
    setTotal(FinancialConstants::COL_YEAR_COST,     AppHelpers::formatAmount(totalCost));
    setTotal(FinancialConstants::COL_YEAR_EXPENSES, AppHelpers::formatAmount(totalExpenses));
    setTotal(FinancialConstants::COL_YEAR_BENEFIT,  AppHelpers::formatAmount(totalBenefit));

    m_yearIncomeTotal = totalIncome;
    m_yearChart->setMonths(monthlyIncome, lastMonth);
    m_yearTotal->setText(tr("%1 total: %2").arg(yyyy, AppHelpers::formatPrice(totalIncome)));
}

void FinancialWindow::onYearlySelectChanged()
{
    loadYearlyTable();
}

void FinancialWindow::onYearlyTableDoubleClicked(int row, int column)
{
    Q_UNUSED(column)
    if (row < 0 || row >= 12) return; // ignore total row

    const int year  = FinancialUIMap::yearlySelect->currentData().toInt();
    const int month = row + 1;
    const QString mm   = QString("%1").arg(month, 2, 10, QChar('0'));
    const QString yyyy = QString::number(year);

    int count = 0;
    const double income = m_api->incomeByMonth(yyyy, mm, &count);
    const QString monthName = TranslationManager::instance().locale()
                                  .standaloneMonthName(month, QLocale::LongFormat);

    AppHelpers::showInfo(this,
                         tr("Transactions: %1\nTotal Income: %2")
                             .arg(count)
                             .arg(AppHelpers::formatPrice(income)),
                         QString("%1 %2").arg(monthName).arg(year));
}

void FinancialWindow::onLogoutClicked()
{
    if (!LogoutManager::confirmAndLogout(this, m_session->sessionId())) {
        return;
    }
    close();
    emit logoutRequested();
}
