#ifndef NOTIFICATIONMANAGER_H
#define NOTIFICATIONMANAGER_H

#include "CoreExport.h"
#include <QObject>
#include <QJsonObject>
#include <QString>
#include <QList>
#include <QSet>

class LocalApi;
class QTimer;

// The notification list of the signed-in user.
// Notifications are derived from the local data (LocalApi::notificationsSync)
// with per-user read and dismissal state. This manager caches the last
// snapshot for the UI, refreshes when any POSBox app reports a change (see
// NotificationBell) or on the periodic timer, forwards read/dismiss actions,
// and chimes when a new unread alert appears.
class CORE_EXPORT NotificationManager : public QObject
{
    Q_OBJECT

public:
    enum class Type {
        OpenTicket = 0,
        ExpenseDue = 1,
        ExpenseOverdue = 2,
        OutOfStock = 3,
        NearlyOutOfStock = 4
    };

    enum class StockFilter {
        All = 0,
        OutOfStock = 1,
        LowStock = 2
    };

    struct Notification {
        QString id;         // "type:context" stable key
        Type type;
        QString title;
        QString message;
        QString action;     // "App|flag|value" deep-link
        StockFilter stockFilter = StockFilter::All;
        bool read;
        QString priority;   // "high" | "normal"
        int contextId;
        QString date;
    };

    explicit NotificationManager(LocalApi* api, QObject* parent = nullptr);

    int unreadCount() const;
    QList<Notification> getAll() const;

    void markAsRead(const QString& id);
    void markAllAsRead();
    // Sticky per user: it stays hidden until its condition changes
    // (which clears the dismissal and re-alerts everyone).
    void dismiss(const QString& id);
    void dismissAll();

    void refreshNow();        // re-derive from the database
    // Coalesces bursts of changes (a sale touches several rows) into one refresh.
    void refreshSoon();

    // What the apps read besides the list (see notification_settings).
    struct Settings {
        int lowStockMin = 3;
    };
    Settings settings() const;

signals:
    void notificationsChanged();

private:
    static Type typeFromString(const QString& str);
    static QString defaultAction(Type type, int contextId);

    LocalApi* m_api;
    Settings m_settings;
    QList<Notification> m_notifications;
    QTimer* m_pushDebounce;
    bool m_synced = false;            // the first sync never chimes
    QSet<QString> m_unreadSeen;       // unread ids of the previous snapshot
};

#endif // NOTIFICATIONMANAGER_H
