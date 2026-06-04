#ifndef NOTIFICATIONPANEL_H
#define NOTIFICATIONPANEL_H

#include "NotificationManager.h"
#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QFrame>
#include <QShowEvent>
#include <QSqlDatabase>

class NotificationPanel : public QFrame
{
    Q_OBJECT

public:
    explicit NotificationPanel(NotificationManager* manager, const QString& username, const QSqlDatabase& db, QWidget* parent = nullptr);
    ~NotificationPanel();

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
    void showEvent(QShowEvent* event) override;

private slots:
    void onRefreshClicked();
    void onMarkAllReadClicked();
    void onNotificationClicked(const QString& id, NotificationManager::Type type, int contextId);

private:
    void refreshNotifications();
    void addNotificationItem(const NotificationManager::Notification& notif);
    void navigateToApp(NotificationManager::Type type, int contextId);

    NotificationManager* m_manager;
    QString m_username;
    QSqlDatabase m_db;
    QVBoxLayout* m_mainLayout;
    QWidget* m_contentWidget;
    QVBoxLayout* m_contentLayout;
    QLabel* m_emptyLabel;
};

#endif
