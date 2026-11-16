#include "NotificationManager.h"
#include "NotificationSound.h"
#include "NotificationActions.h"
#include "LocalApi.h"
#include <QJsonArray>
#include <QTimer>
#include <QDebug>

NotificationManager::NotificationManager(LocalApi* api, QObject* parent)
    : QObject(parent)
    , m_api(api)
{
    m_pushDebounce = new QTimer(this);
    m_pushDebounce->setSingleShot(true);
    m_pushDebounce->setInterval(500);
    connect(m_pushDebounce, &QTimer::timeout, this, &NotificationManager::refreshNow);
}

void NotificationManager::refreshSoon()
{
    m_pushDebounce->start();
}

int NotificationManager::unreadCount() const
{
    int count = 0;
    for (const auto& n : m_notifications) {
        if (!n.read) ++count;
    }
    return count > 99 ? 99 : count;
}

QList<NotificationManager::Notification> NotificationManager::getAll() const
{
    return m_notifications;
}

void NotificationManager::markAsRead(const QString& id)
{
    if (!id.isEmpty() && m_api && !m_api->notificationRead(id)) {
        // Keep the local update; the next sync reconciles.
        qWarning() << "Notification mark-read failed:" << m_api->lastError();
    }

    for (auto& n : m_notifications) {
        if (n.id == id) {
            n.read = true;
            break;
        }
    }
    emit notificationsChanged();
    m_pushDebounce->start();
}

void NotificationManager::markAllAsRead()
{
    if (m_api && !m_api->notificationsReadAll())
        qWarning() << "Notification mark-all-read failed:" << m_api->lastError();

    for (auto& n : m_notifications) {
        n.read = true;
    }
    emit notificationsChanged();
    m_pushDebounce->start();
}

void NotificationManager::dismiss(const QString& id)
{
    if (id.isEmpty())
        return;
    if (m_api && !m_api->notificationDismiss(id)) {
        // Re-read what the database has.
        qWarning() << "Notification dismiss failed:" << m_api->lastError();
        m_pushDebounce->start();
        return;
    }

    m_notifications.removeIf([&id](const Notification& n) { return n.id == id; });
    emit notificationsChanged();
    m_pushDebounce->start();
}

void NotificationManager::dismissAll()
{
    if (m_api && !m_api->notificationsDismissAll()) {
        qWarning() << "Notification dismiss-all failed:" << m_api->lastError();
        m_pushDebounce->start();
        return;
    }

    m_notifications.clear();
    emit notificationsChanged();
    m_pushDebounce->start();
}

void NotificationManager::refreshNow()
{
    if (!m_api) return;

    QJsonObject resp = m_api->notificationsSync();
    if (!m_api->ok()) {
        // Keep the last snapshot; the caller retries on its next cycle.
        qWarning() << "Notification sync failed:" << m_api->lastError();
        return;
    }

    const QJsonObject s = resp.value("settings").toObject();
    m_settings.lowStockMin = s.value("low_stock_min").toInt(3);

    m_notifications.clear();
    for (const QJsonValue& v : resp.value("notifications").toArray()) {
        const QJsonObject o = v.toObject();
        Notification n;
        n.id = o.value("id").toString();
        n.type = typeFromString(o.value("type").toString());
        n.contextId = o.value("context_id").toInt();
        n.title = o.value("title").toString();
        n.message = o.value("message").toString();
        n.read = o.value("read").toBool();
        n.priority = o.value("priority").toString();
        n.date = o.value("date").toString();

        n.action = o.value("action").toString();
        if (n.action.isEmpty())
            n.action = defaultAction(n.type, n.contextId);

        switch (n.type) {
        case Type::OutOfStock:
            n.stockFilter = StockFilter::OutOfStock;
            break;
        case Type::NearlyOutOfStock:
            n.stockFilter = StockFilter::LowStock;
            break;
        default:
            break;
        }

        m_notifications.append(n);
    }

    // A new unread alert (or one whose condition changed) chimes once.
    QSet<QString> unread;
    for (const Notification& n : m_notifications)
        if (!n.read)
            unread.insert(n.id);
    bool fresh = false;
    for (const QString& id : unread)
        if (!m_unreadSeen.contains(id)) { fresh = true; break; }
    if (m_synced && fresh)
        NotificationSound::play();
    m_unreadSeen = unread;
    m_synced = true;

    emit notificationsChanged();
}

NotificationManager::Settings NotificationManager::settings() const
{
    return m_settings;
}

NotificationManager::Type NotificationManager::typeFromString(const QString& str)
{
    if (str == "open_ticket") return Type::OpenTicket;
    if (str == "expense_due") return Type::ExpenseDue;
    if (str == "expense_overdue") return Type::ExpenseOverdue;
    if (str == "out_of_stock") return Type::OutOfStock;
    if (str == "nearly_out_of_stock") return Type::NearlyOutOfStock;
    return Type::OpenTicket;
}

QString NotificationManager::defaultAction(Type type, int contextId)
{
    switch (type) {
    case Type::OpenTicket:
        return NotificationActions::build("Register", "open-ticket", contextId);
    case Type::ExpenseDue:
    case Type::ExpenseOverdue:
        return NotificationActions::build("Financial", "expense", contextId);
    case Type::OutOfStock:
        return NotificationActions::build("Inventory", "out", 1);
    case Type::NearlyOutOfStock:
        return NotificationActions::build("Inventory", "low", 2);
    }
    return {};
}
