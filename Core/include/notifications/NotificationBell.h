#ifndef NOTIFICATIONBELL_H
#define NOTIFICATIONBELL_H

#include "CoreExport.h"
#include <QObject>
#include <QString>

class ChangeNotifier;
class NotificationManager;
class NotificationPanel;
class QLabel;
class QPushButton;
class QTimer;
class QWidget;
class LocalApi;

// The notification bell every app window shows: the derived
// notification list, its periodic refresh, the unread badge on the button and
// the drop-down panel.
class CORE_EXPORT NotificationBell : public QObject
{
    Q_OBJECT

public:
    NotificationBell(LocalApi* api, QPushButton* button, const QString& username,
                     ChangeNotifier* notifier, QWidget* window);

    NotificationManager* manager() const { return m_manager; }

    // Fires at each periodic refresh, for windows that refresh more with it.
    QTimer* refreshTimer() const { return m_refreshTimer; }

    void refreshNow();

    // Shows the panel under the bell button, or hides it if it is showing.
    void togglePanel();

private:
    void updateBadge();

    QPushButton* m_button;
    QString m_username;
    ChangeNotifier* m_notifier;
    QWidget* m_window;
    NotificationManager* m_manager;
    QTimer* m_refreshTimer;
    NotificationPanel* m_panel = nullptr;
    QLabel* m_badge = nullptr;
};

#endif // NOTIFICATIONBELL_H
