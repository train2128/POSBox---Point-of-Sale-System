#ifndef FINANCIALWINDOW_H
#define FINANCIALWINDOW_H

#include <QMainWindow>
#include <QString>

class QFrame;
class QHBoxLayout;
class QLabel;
class UserSession;
class LocalApi;
class NotificationBell;
class ChangeNotifier;
class WeekChart;
class TopSellersChart;
class YearChart;

namespace Ui {
class FinancialWindow;
}

class FinancialWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit FinancialWindow(UserSession* session,
                             LocalApi* api,
                             ChangeNotifier* notifier = nullptr,
                             QWidget* parent = nullptr);
    ~FinancialWindow();

    // Handles a "Financial|flag|id" deep-link (from the Launcher navigation stream).
    void handleNavigation(const QString& action);

private slots:
    void onLogoutClicked();

    // Tab 1 — Overview
    void onMonthYearChanged();
    void onAddExpenseClicked();
    void onDeleteExpenseClicked();

    // Tab 2 — Yearly Summary
    void onYearlySelectChanged();
    void onYearlyTableDoubleClicked(int row, int column);

signals:
    void logoutRequested();

private:
    // One summary number on the Overview
    struct Kpi {
        QFrame* card = nullptr;
        QFrame* bar = nullptr;
        QLabel* title = nullptr;
        QLabel* sub = nullptr;
        QLabel* value = nullptr;
    };

    void buildLayout();
    void addKpiCard(QHBoxLayout* row, const char* tone, Kpi& kpi);
    void setKpiValue(Kpi& kpi, double value);
    void retranslateUi();

    void setupConnections();
    void updateUserDisplay();

    // Tab 1 loaders
    void loadMonthlySummary();
    void loadWeeklyTable();
    void loadTopSalesTable();
    void loadOneTimeExpenses();

    // Tab 2 loaders
    void loadYearlyTable();

    int selectedOneTimeExpenseId() const;

    Ui::FinancialWindow* ui;
    UserSession*         m_session;
    LocalApi*            m_api;
    ChangeNotifier*      m_notifier;

    // Controls added by buildLayout()
    Kpi m_kpiIncome;
    Kpi m_kpiCost;
    Kpi m_kpiBenefit;
    Kpi m_kpiExpenses;
    Kpi m_kpiRest;
    QLabel* m_periodTitle = nullptr;
    QLabel* m_weekTitle = nullptr;
    QLabel* m_weekLegend = nullptr;
    QLabel* m_topTitle = nullptr;
    QLabel* m_expenseTitle = nullptr;
    QLabel* m_expenseTotalCaption = nullptr;
    QLabel* m_chartTitle = nullptr;
    QLabel* m_yearCaption = nullptr;
    WeekChart* m_weekChart = nullptr;
    TopSellersChart* m_topSellers = nullptr;
    QLabel* m_expenseTotal = nullptr;
    QLabel* m_yearTotal = nullptr;
    YearChart* m_yearChart = nullptr;
    double m_yearIncomeTotal = 0.0;

    // Notification system
    NotificationBell* m_bell = nullptr;
};

#endif // FINANCIALWINDOW_H
