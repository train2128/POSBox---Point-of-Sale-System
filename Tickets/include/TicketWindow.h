#ifndef TICKETWINDOW_H
#define TICKETWINDOW_H

#include <QMainWindow>
#include <QDate>

class UserSession;
class LocalApi;
class LocalImages;
class ChangeNotifier;
class NotificationBell;
class QStandardItemModel;
class QSortFilterProxyModel;
class TicketPrintManager;
class ThermalPrinter;
class QLabel;
class QTimer;
class QPushButton;

class QFrame;
namespace Ui {
class TicketWindow;
}

class TicketWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit TicketWindow(UserSession* session,
                          LocalApi* api,
                          LocalImages* images,
                          ChangeNotifier* notifier,
                          QWidget* parent = nullptr);
    ~TicketWindow();

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
    void buildLayout();
    void showReceiptPlaceholder();
    void showReceiptFor(int sourceRow);
    void fitReceiptList();

    Ui::TicketWindow*      ui;
    UserSession*           m_session;
    LocalApi*             m_api;
    LocalImages*          m_images;
    ChangeNotifier*        m_notifier;
    QStandardItemModel*    m_model;
    QSortFilterProxyModel* m_proxy;
    int                    m_selectedTicketId;
    TicketPrintManager*    m_printManager;

    // Header pills
    QLabel* m_ticketCountLabel = nullptr;
    QLabel* m_refundedCountLabel = nullptr;

    // Receipt panel
    QFrame*  m_receiptPaper = nullptr;
    QLabel*  m_receiptPlaceholder = nullptr;
    QWidget* m_receiptBody = nullptr;
    QLabel*  m_receiptShopName = nullptr;
    QLabel*  m_receiptShopAddress = nullptr;
    QLabel*  m_receiptNumber = nullptr;
    QLabel*  m_receiptTime = nullptr;
    QLabel*  m_receiptSeller = nullptr;
    QLabel*  m_receiptSubtotal = nullptr;
    QLabel*  m_receiptDiscount = nullptr;
    QWidget* m_receiptDiscountRow = nullptr;
    QLabel*  m_receiptTotal = nullptr;
    bool     m_shopLoaded = false;

    // Notification system
    NotificationBell* m_bell = nullptr;
};

#endif // TICKETWINDOW_H
