#ifndef TICKETWINDOW_H
#define TICKETWINDOW_H

#include <QMainWindow>
#include <QDate>

class UserSession;
class DatabaseManager;
class ChangeNotifier;
class QStandardItemModel;
class QSortFilterProxyModel;
class TicketPrintManager;
class ThermalPrinter;

namespace Ui {
class TicketWindow;
}

class TicketWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit TicketWindow(UserSession* session,
                          DatabaseManager* dbManager,
                          ChangeNotifier* notifier,
                          QWidget* parent = nullptr);
    ~TicketWindow();

    void retranslateUi();

private slots:
    void onLogoutClicked();
    void onDateChanged(const QDate& date);
    void onSearchChanged(const QString& text);
    void onTicketSelected(const QModelIndex& current, const QModelIndex& previous);
    void onDeleteClicked();
    void onRefundClicked();
    void onEditClicked();
    void onPrintClicked();

signals:
    void logoutRequested();

private:
    void setupUI();
    void setupTable();
    void updateUserDisplay();
    void loadTickets(const QDate& date);
    void loadTicketItems(int ticketId);
    void clearTicketCart();
    void setActionButtonsEnabled(bool enabled);
    void restoreStock(int ticketId);

    Ui::TicketWindow*      ui;
    UserSession*           m_session;
    DatabaseManager*       m_db;
    ChangeNotifier*        m_notifier;
    QStandardItemModel*    m_model;
    QSortFilterProxyModel* m_proxy;
    int                    m_selectedTicketId;
    TicketPrintManager*    m_printManager;
};

#endif // TICKETWINDOW_H
