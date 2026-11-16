#ifndef CHANGENOTIFIER_H
#define CHANGENOTIFIER_H

#include "CoreExport.h"
#include <QObject>
#include <QSharedMemory>
#include <QTimer>
#include <QDateTime>

// Change notification types
enum class ChangeType : int {
    NONE = 0,
    PRODUCT_CHANGED = 1,
    CATEGORY_CHANGED = 2,
    TICKET_CREATED = 3,
    TICKET_UPDATED = 4,
    EXPENSE_UPDATED = 5,
    ACCOUNT_CHANGED = 6,
    DISCOUNT_CHANGED = 7,
    PROFILE_UPDATED = 8,
    TICKET_EDIT_REQUESTED = 9,
    SESSION_LOGOUT_REQUESTED = 10,
    APP_OPEN_REQUESTED = 11,
    APP_NAVIGATE_REQUESTED = 12
};

// Shared memory structure for notifications
struct ChangeNotification {
    ChangeType type;
    int recordId;
    qint64 timestamp;
    char extra[32]; // Extra data (username, etc.)
};

class CORE_EXPORT ChangeNotifier : public QObject
{
    Q_OBJECT

public:
    explicit ChangeNotifier(QObject* parent = nullptr);
    ~ChangeNotifier();

    // Initialize shared memory
    bool initialize();

    // Notify other apps of a change
    bool notifyChange(ChangeType type, int recordId, const QString& extra = QString());

    // Start/stop polling for changes
    void startPolling(int intervalMs = 2000);
    void stopPolling();

signals:
    // Emitted when changes detected
    void productChanged(int productId);
    void categoryChanged(int categoryId);
    void ticketCreated(int ticketId);
    void ticketUpdated(int ticketId);
    void expenseUpdated(int expenseId);
    void discountChanged(int discountId);
    void sessionLogoutRequested();
    void appOpenRequested(const QString& request);
    void appNavigateRequested(const QString& request);

private slots:
    void checkForChanges();

private:
    void deliver(const ChangeNotification& notification);

    QSharedMemory m_sharedMemory;
    QTimer m_pollTimer;
    qint64 m_lastCheckTimestamp;

    static const int MAX_NOTIFICATIONS = 50;
    static constexpr const char* SHARED_MEMORY_KEY = "POSBoxChangeNotifier";

    // Get current notification buffer
    ChangeNotification* getNotificationBuffer();
    int getNotificationCount();
    bool addNotification(const ChangeNotification& notification);
};

#endif // CHANGENOTIFIER_H
