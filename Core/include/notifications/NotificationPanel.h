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

class NotificationManager;
class ChangeNotifier;

class NotificationPanel : public QFrame
{
    Q_OBJECT

public:
    explicit NotificationPanel(NotificationManager* manager,
                               const QString& username,
                               ChangeNotifier* notifier = nullptr,
                               QWidget* parent = nullptr);
    ~NotificationPanel();

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private slots:
    void onRefreshClicked();
    void onMarkAllReadClicked();
    void onClearAllClicked();

private:
    void refreshNotifications();
    void addNotificationItem(const NotificationManager::Notification& notif);
    void navigateToApp(const QString& action, int contextId);

    NotificationManager* m_manager;
    ChangeNotifier*      m_notifier;
    QString m_username;
    QVBoxLayout* m_mainLayout;
    QWidget* m_contentWidget;
    QVBoxLayout* m_contentLayout;
    QWidget* m_emptyState;
    QWidget* m_header;
    QLabel* m_bell;
    QLabel* m_subtitle;
    QPushButton* m_markAllBtn;
    QPushButton* m_clearAllBtn;
    QScrollArea* m_scrollArea;
};

#endif
