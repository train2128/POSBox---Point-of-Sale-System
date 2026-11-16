#ifndef NOTIFICATIONSSERVICE_H
#define NOTIFICATIONSSERVICE_H

#include <QJsonObject>
#include <QString>

// Notifications derived from live data (internal to Core).
//
// A notification exists only while its condition is true: every sync
// re-derives the list, adds what is new, updates what changed and removes
// what was resolved. Read and dismissal state are per user
// (notification_reads); when a grouped alert's members change (e.g. another
// product runs out) everyone sees it again as unread.
class NotificationsService
{
public:
    // {ok, notifications:[{id,type,context_id,title,message,read,date,action,priority}], settings:{...}}
    static QJsonObject sync(const QString& username, QString* errorOut);
    static QJsonObject markRead(const QString& username, const QString& id, QString* errorOut);
    static QJsonObject markAllRead(const QString& username, QString* errorOut);
    // Dismissal is sticky: hidden for this user until the condition changes.
    static QJsonObject dismiss(const QString& username, const QString& id, QString* errorOut);
    static QJsonObject dismissAll(const QString& username, QString* errorOut);
};

#endif // NOTIFICATIONSSERVICE_H
