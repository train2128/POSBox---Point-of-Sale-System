#include "NotificationBell.h"
#include "ChangeNotifier.h"
#include "NotificationBadge.h"
#include "NotificationManager.h"
#include "NotificationPanel.h"
#include <QPushButton>
#include <QTimer>

namespace {
constexpr int RefreshIntervalMs = 3 * 60 * 1000;
}

NotificationBell::NotificationBell(LocalApi* api, QPushButton* button, const QString& username,
                                   ChangeNotifier* notifier, QWidget* window)
    : QObject(window)
    , m_button(button)
    , m_username(username)
    , m_notifier(notifier)
    , m_window(window)
    , m_manager(new NotificationManager(api, this))
    , m_refreshTimer(new QTimer(this))
{
    m_manager->refreshNow();

    m_refreshTimer->setInterval(RefreshIntervalMs);
    connect(m_refreshTimer, &QTimer::timeout, m_manager, &NotificationManager::refreshNow);
    m_refreshTimer->start();

    // Any change made in a POSBox app on this PC (this one included) may
    // start or end an alert: re-derive shortly after.
    if (m_notifier) {
        auto soon = [this]() { m_manager->refreshSoon(); };
        connect(m_notifier, &ChangeNotifier::productChanged, this, soon);
        connect(m_notifier, &ChangeNotifier::categoryChanged, this, soon);
        connect(m_notifier, &ChangeNotifier::ticketCreated, this, soon);
        connect(m_notifier, &ChangeNotifier::ticketUpdated, this, soon);
        connect(m_notifier, &ChangeNotifier::discountChanged, this, soon);
    }

    connect(m_manager, &NotificationManager::notificationsChanged, this, &NotificationBell::updateBadge);
    updateBadge();
}

void NotificationBell::refreshNow()
{
    m_manager->refreshNow();
}

void NotificationBell::togglePanel()
{
    if (!m_panel)
        m_panel = new NotificationPanel(m_manager, m_username, m_notifier, m_window);

    if (m_panel->isVisible()) {
        m_panel->hide();
    } else {
        const QPoint buttonPos = m_button->mapToGlobal(QPoint(0, 0));
        m_panel->move(buttonPos.x() - m_panel->width() + m_button->width(),
                      buttonPos.y() + m_button->height());
        m_panel->show();
    }
}

void NotificationBell::updateBadge()
{
    if (!m_badge)
        m_badge = NotificationBadge::createBadge(m_button);
    NotificationBadge::updateBadge(m_badge, m_manager->unreadCount());
}
