#ifndef FINANCIALWINDOW_H
#define FINANCIALWINDOW_H

#include <QMainWindow>
#include <QDate>

class UserSession;
class DatabaseManager;
class NotificationManager;
class NotificationPanel;

namespace Ui {
class FinancialWindow;
}

class FinancialWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit FinancialWindow(UserSession* session,
                             DatabaseManager* db,
                             QWidget* parent = nullptr);
    ~FinancialWindow();

private slots:
    void onNotificationClicked();
    void onLogoutClicked();

    // Tab 1 — Overview
    void onMonthYearChanged();
    void onAddExpenseClicked();
    void onDeleteExpenseClicked();

    // Tab 2 — Yearly Summary
    void onYearlyTableDoubleClicked(int row, int column);

signals:
    void logoutRequested();

private:
    void setupUI();
    void setupConnections();
    void updateUserDisplay();

    // Tab 1 loaders
    void loadMonthlySummary();
    void loadWeeklyTable();
    void loadTopSalesTable();
    void loadOneTimeExpenses();

    // Tab 2 loaders
    void loadYearlyTable();

    // Helpers
    int selectedOneTimeExpenseId() const;

    Ui::FinancialWindow* ui;
    UserSession*         m_session;
    DatabaseManager*     m_db;

    // Notification system
    NotificationManager* m_notificationManager;
    NotificationPanel* m_notificationPanel;
    void updateNotificationBadge();
};

#endif // FINANCIALWINDOW_H
