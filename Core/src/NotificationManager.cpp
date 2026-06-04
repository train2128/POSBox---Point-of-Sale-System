#include "NotificationManager.h"
#include "DatabaseManager.h"
#include "CommonConfig.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QUuid>
#include <QDate>
#include <QDebug>

NotificationManager::NotificationManager(DatabaseManager* db, QObject* parent)
    : QObject(parent)
    , m_db(db)
{
    const QString appDataPath = POSConfig::getAppDataPath();
    m_notificationsPath = appDataPath + "notifications.json";
    
    loadNotifications();
    m_settings = NotificationSettings::load(appDataPath + "notification_settings.json");
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
    for (auto& n : m_notifications) {
        if (n.id == id) {
            n.read = true;
            break;
        }
    }
    saveNotifications();
    emit notificationsChanged();
}

void NotificationManager::markAllAsRead()
{
    for (auto& n : m_notifications) {
        n.read = true;
    }
    saveNotifications();
    emit notificationsChanged();
}

void NotificationManager::removeNotification(const QString& id)
{
    m_notifications.removeIf([&id](const Notification& n) { return n.id == id; });
    saveNotifications();
    emit notificationsChanged();
}

void NotificationManager::removeByContextId(Type type, int contextId)
{
    m_notifications.removeIf([type, contextId](const Notification& n) {
        return n.type == type && n.contextId == contextId;
    });
    saveNotifications();
    emit notificationsChanged();
}

void NotificationManager::setSettings(const NotificationSettings& settings)
{
    m_settings = settings;
    const QString appDataPath = POSConfig::getAppDataPath();
    m_settings.save(appDataPath + "notification_settings.json");
}

NotificationSettings NotificationManager::settings() const
{
    return m_settings;
}

void NotificationManager::checkAndGenerate()
{
    const QString today = QDate::currentDate().toString(Qt::ISODate);
    
    if (!shouldGenerateToday(m_lastCheckDate)) {
        return;
    }
    
    m_lastCheckDate = today;
    generateAllNotifications();
}

void NotificationManager::refreshNow()
{
    m_settings = NotificationSettings::load(POSConfig::getAppDataPath() + "notification_settings.json");
    generateAllNotifications();
}

void NotificationManager::generateAllNotifications()
{
    QList<QPair<Type, int>> validIds;
    
    if (m_settings.openTicket.enabled) {
        collectValidOpenTicketIds(validIds);
    }
    if (m_settings.expenseDue.enabled) {
        collectValidExpenseDueIds(validIds);
    }
    if (m_settings.expenseOverdue.enabled) {
        collectValidExpenseOverdueIds(validIds);
    }
    if (m_settings.outOfStock.enabled) {
        collectValidOutOfStockIds(validIds);
    }
    if (m_settings.nearlyOutOfStock.enabled) {
        collectValidNearlyOutOfStockIds(validIds);
    }
    
    removeInvalidNotifications(validIds);
    
    if (m_settings.openTicket.enabled) {
        generateOpenTicketNotifications();
    }
    if (m_settings.expenseDue.enabled) {
        generateExpenseDueNotifications();
    }
    if (m_settings.expenseOverdue.enabled) {
        generateExpenseOverdueNotifications();
    }
    if (m_settings.outOfStock.enabled) {
        generateOutOfStockNotifications();
    }
    if (m_settings.nearlyOutOfStock.enabled) {
        generateNearlyOutOfStockNotifications();
    }
    
    saveNotifications();
    emit notificationsChanged();
}

void NotificationManager::removeInvalidNotifications(const QList<QPair<Type, int>>& validIds)
{
    QSet<QPair<Type, int>> validSet;
    for (const auto& v : validIds) {
        validSet.insert(v);
    }
    
    m_notifications.removeIf([&validSet](const Notification& n) {
        QPair<Type, int> key(n.type, n.contextId);
        return !validSet.contains(key);
    });
}

void NotificationManager::collectValidOpenTicketIds(QList<QPair<Type, int>>& validIds)
{
    if (!m_db) return;
    
    const int days = m_settings.openTicket.daysThreshold;
    const QString today = QDate::currentDate().toString(Qt::ISODate);
    
    QSqlQuery q(m_db->database());
    q.prepare(
        "SELECT id FROM tickets "
        "WHERE status = 'OPEN' "
        "AND date(date, '+' || ? || ' days') <= ?");
    q.addBindValue(days);
    q.addBindValue(today);
    
    if (!q.exec()) return;
    
    while (q.next()) {
        validIds.append(qMakePair(Type::OpenTicket, q.value("id").toInt()));
    }
}

void NotificationManager::collectValidExpenseDueIds(QList<QPair<Type, int>>& validIds)
{
    if (!m_db) return;
    
    const int daysBefore = m_settings.expenseDue.daysBefore;
    const QDate targetDate = QDate::currentDate().addDays(daysBefore);
    
    QSqlQuery q(m_db->database());
    q.prepare(
        "SELECT id FROM recurring_expenses "
        "WHERE is_active = 1 "
        "AND next_due_date <= ?");
    q.addBindValue(targetDate.toString(Qt::ISODate));
    
    if (!q.exec()) return;
    
    while (q.next()) {
        validIds.append(qMakePair(Type::ExpenseDue, q.value("id").toInt()));
    }
}

void NotificationManager::collectValidExpenseOverdueIds(QList<QPair<Type, int>>& validIds)
{
    if (!m_db) return;
    
    const QString today = QDate::currentDate().toString(Qt::ISODate);
    
    QSqlQuery q(m_db->database());
    q.prepare(
        "SELECT id FROM recurring_expenses "
        "WHERE is_active = 1 "
        "AND next_due_date < ?");
    q.addBindValue(today);
    
    if (!q.exec()) return;
    
    while (q.next()) {
        validIds.append(qMakePair(Type::ExpenseOverdue, q.value("id").toInt()));
    }
}

void NotificationManager::collectValidOutOfStockIds(QList<QPair<Type, int>>& validIds)
{
    if (!m_db) return;
    
    QSqlQuery q(m_db->database());
    q.prepare(
        "SELECT id FROM products "
        "WHERE track_stock = 1 AND stock_quantity = 0 AND is_service = 0");
    
    if (!q.exec()) return;
    
    while (q.next()) {
        validIds.append(qMakePair(Type::OutOfStock, q.value("id").toInt()));
    }
}

void NotificationManager::collectValidNearlyOutOfStockIds(QList<QPair<Type, int>>& validIds)
{
    if (!m_db) return;
    
    QSqlQuery q(m_db->database());
    q.prepare(
        "SELECT id FROM products "
        "WHERE track_stock = 1 AND stock_quantity > 0 AND stock_quantity <= min_stock_alert "
        "AND min_stock_alert > 0 AND is_service = 0");
    
    if (!q.exec()) return;
    
    while (q.next()) {
        validIds.append(qMakePair(Type::NearlyOutOfStock, q.value("id").toInt()));
    }
}

bool NotificationManager::shouldGenerateToday(const QString& lastDate) const
{
    if (lastDate.isEmpty()) return true;
    
    QDate last = QDate::fromString(lastDate, Qt::ISODate);
    QDate today = QDate::currentDate();
    
    return last.daysTo(today) >= 1;
}

void NotificationManager::generateOpenTicketNotifications()
{
    if (!m_db) return;
    
    const int days = m_settings.openTicket.daysThreshold;
    const int frequency = m_settings.openTicket.frequencyDays;
    const QString today = QDate::currentDate().toString(Qt::ISODate);
    
    QSqlQuery q(m_db->database());
    q.prepare(
        "SELECT id, ticket_number, date FROM tickets "
        "WHERE status = 'OPEN' "
        "AND date(date, '+' || ? || ' days') <= ?");
    q.addBindValue(days);
    q.addBindValue(today);
    
    if (!q.exec()) return;
    
    while (q.next()) {
        const int ticketId = q.value("id").toInt();
        const QString ticketNumber = q.value("ticket_number").toString();
        const QString ticketDate = q.value("date").toString();
        
        QDate created = QDate::fromString(ticketDate, Qt::ISODate);
        int daysOpen = created.daysTo(QDate::currentDate());
        
        if (daysOpen % frequency == 0) {
            const QString title = "Open Ticket";
            const QString msg = QString("Ticket #%1 has been open for %2 days").arg(ticketNumber).arg(daysOpen);
            addNotification(Type::OpenTicket, title, msg, ticketId);
        }
    }
}

void NotificationManager::generateExpenseDueNotifications()
{
    if (!m_db) return;
    
    const int daysBefore = m_settings.expenseDue.daysBefore;
    const QString today = QDate::currentDate().toString(Qt::ISODate);
    const QDate targetDate = QDate::currentDate().addDays(daysBefore);
    
    QSqlQuery q(m_db->database());
    q.prepare(
        "SELECT id, name, next_due_date FROM recurring_expenses "
        "WHERE is_active = 1 "
        "AND next_due_date <= ?");
    q.addBindValue(targetDate.toString(Qt::ISODate));
    
    if (!q.exec()) return;
    
    while (q.next()) {
        const int expId = q.value("id").toInt();
        const QString name = q.value("name").toString();
        const QString dueDate = q.value("next_due_date").toString();
        
        QDate due = QDate::fromString(dueDate, Qt::ISODate);
        int daysUntil = QDate::currentDate().daysTo(due);
        
        QString title = daysUntil < 0 ? "Expense Overdue" : "Expense Due Soon";
        QString msg = daysUntil < 0
            ? QString("%1 is %2 days overdue").arg(name).arg(qAbs(daysUntil))
            : QString("%1 is due in %2 days").arg(name).arg(daysUntil);
        
        addNotification(Type::ExpenseDue, title, msg, expId);
    }
}

void NotificationManager::generateExpenseOverdueNotifications()
{
    if (!m_db) return;
    
    const int daysOverdue = m_settings.expenseOverdue.daysOverdue;
    const QString today = QDate::currentDate().toString(Qt::ISODate);
    
    QSqlQuery q(m_db->database());
    q.prepare(
        "SELECT id, name, next_due_date FROM recurring_expenses "
        "WHERE is_active = 1 "
        "AND next_due_date < ?");
    q.addBindValue(today);
    
    if (!q.exec()) return;
    
    while (q.next()) {
        const int expId = q.value("id").toInt();
        const QString name = q.value("name").toString();
        const QString dueDate = q.value("next_due_date").toString();
        
        QDate due = QDate::fromString(dueDate, Qt::ISODate);
        int daysOver = qAbs(QDate::currentDate().daysTo(due));
        
        if (daysOver >= daysOverdue && daysOver % daysOver == 0) {
            const QString title = "Expense Overdue";
            const QString msg = QString("%1 is %2 days overdue").arg(name).arg(daysOver);
            addNotification(Type::ExpenseOverdue, title, msg, expId);
        }
    }
}

void NotificationManager::generateOutOfStockNotifications()
{
    if (!m_db) return;
    
    QSqlQuery q(m_db->database());
    q.prepare(
        "SELECT id, name FROM products "
        "WHERE track_stock = 1 AND stock_quantity = 0 AND is_service = 0");
    
    if (!q.exec()) return;
    
    while (q.next()) {
        const int productId = q.value("id").toInt();
        const QString name = q.value("name").toString();
        
        const QString title = "Out of Stock";
        const QString msg = QString("%1 is out of stock").arg(name);
        addNotification(Type::OutOfStock, title, msg, productId);
    }
}

void NotificationManager::generateNearlyOutOfStockNotifications()
{
    if (!m_db) return;
    
    QSqlQuery q(m_db->database());
    q.prepare(
        "SELECT id, name, stock_quantity, min_stock_alert FROM products "
        "WHERE track_stock = 1 AND stock_quantity > 0 AND stock_quantity <= min_stock_alert "
        "AND min_stock_alert > 0 AND is_service = 0");
    
    if (!q.exec()) return;
    
    while (q.next()) {
        const int productId = q.value("id").toInt();
        const QString name = q.value("name").toString();
        const int stock = q.value("stock_quantity").toInt();
        
        const QString title = "Low Stock";
        const QString msg = QString("%1 is running low (%2 remaining)").arg(name).arg(stock);
        addNotification(Type::NearlyOutOfStock, title, msg, productId);
    }
}

QString NotificationManager::generateId() const
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

void NotificationManager::addNotification(Type type, const QString& title, const QString& message, int contextId)
{
    for (const auto& n : m_notifications) {
        if (n.type == type && n.contextId == contextId) {
            return;
        }
    }
    
    Notification notif;
    notif.id = generateId();
    notif.type = type;
    notif.title = title;
    notif.message = message;
    notif.read = false;
    notif.contextId = contextId;
    notif.date = QDate::currentDate().toString(Qt::ISODate);
    
    m_notifications.append(notif);
}

void NotificationManager::loadNotifications()
{
    QFile file(m_notificationsPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    
    QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    file.close();
    
    QJsonArray arr = root.value("notifications").toArray();
    
    for (const auto& val : arr) {
        QJsonObject obj = val.toObject();
        Notification n;
        n.id = obj.value("id").toString();
        
        const QString typeStr = obj.value("type").toString();
        if (typeStr == "open_ticket") n.type = Type::OpenTicket;
        else if (typeStr == "expense_due") n.type = Type::ExpenseDue;
        else if (typeStr == "expense_overdue") n.type = Type::ExpenseOverdue;
        else if (typeStr == "out_of_stock") n.type = Type::OutOfStock;
        else if (typeStr == "nearly_out_of_stock") n.type = Type::NearlyOutOfStock;
        
        n.title = obj.value("title").toString();
        n.message = obj.value("message").toString();
        n.read = obj.value("read").toBool();
        n.contextId = obj.value("context_id").toInt();
        n.date = obj.value("date").toString();
        
        m_notifications.append(n);
    }
    
    if (root.contains("last_check_date")) {
        m_lastCheckDate = root.value("last_check_date").toString();
    }
}

void NotificationManager::saveNotifications()
{
    QJsonArray arr;
    for (const auto& n : m_notifications) {
        QJsonObject obj;
        obj["id"] = n.id;
        
        switch (n.type) {
            case Type::OpenTicket: obj["type"] = "open_ticket"; break;
            case Type::ExpenseDue: obj["type"] = "expense_due"; break;
            case Type::ExpenseOverdue: obj["type"] = "expense_overdue"; break;
            case Type::OutOfStock: obj["type"] = "out_of_stock"; break;
            case Type::NearlyOutOfStock: obj["type"] = "nearly_out_of_stock"; break;
        }
        
        obj["title"] = n.title;
        obj["message"] = n.message;
        obj["read"] = n.read;
        obj["context_id"] = n.contextId;
        obj["date"] = n.date;
        
        arr.append(obj);
    }
    
    QJsonObject root;
    root["notifications"] = arr;
    root["last_check_date"] = m_lastCheckDate;
    
    QDir().mkpath(QFileInfo(m_notificationsPath).absolutePath());
    QFile file(m_notificationsPath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson());
        file.close();
    }
}
