#ifndef NOTIFICATIONMANAGER_H
#define NOTIFICATIONMANAGER_H

#include "NotificationSettings.h"
#include <QObject>
#include <QString>
#include <QDate>
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>
#include <QPair>

class DatabaseManager;
class UserSession;

class NotificationManager : public QObject
{
    Q_OBJECT

public:
    enum class Type {
        OpenTicket,
        ExpenseDue,
        ExpenseOverdue,
        OutOfStock,
        NearlyOutOfStock
    };

    struct Notification {
        QString id;
        Type type;
        QString title;
        QString message;
        bool read;
        int contextId;
        QString date;
    };

    explicit NotificationManager(DatabaseManager* db, QObject* parent = nullptr);
    
    int unreadCount() const;
    QList<Notification> getAll() const;
    
    void markAsRead(const QString& id);
    void markAllAsRead();
    void removeNotification(const QString& id);
    void removeByContextId(Type type, int contextId);
    
    void checkAndGenerate();
    void refreshNow();

private:
    void generateAllNotifications();
    void removeInvalidNotifications(const QList<QPair<Type, int>>& validIds);
    void collectValidOpenTicketIds(QList<QPair<Type, int>>& validIds);
    void collectValidExpenseDueIds(QList<QPair<Type, int>>& validIds);
    void collectValidExpenseOverdueIds(QList<QPair<Type, int>>& validIds);
    void collectValidOutOfStockIds(QList<QPair<Type, int>>& validIds);
    void collectValidNearlyOutOfStockIds(QList<QPair<Type, int>>& validIds);
    void setSettings(const NotificationSettings& settings);
    NotificationSettings settings() const;

signals:
    void notificationsChanged();

private:
    void loadNotifications();
    void saveNotifications();
    bool shouldGenerateToday(const QString& lastDate) const;
    void generateOpenTicketNotifications();
    void generateExpenseDueNotifications();
    void generateExpenseOverdueNotifications();
    void generateOutOfStockNotifications();
    void generateNearlyOutOfStockNotifications();
    
    QString generateId() const;
    void addNotification(Type type, const QString& title, const QString& message, int contextId);

    DatabaseManager* m_db;
    NotificationSettings m_settings;
    QList<Notification> m_notifications;
    QString m_lastCheckDate;
    QString m_notificationsPath;
};

#endif
