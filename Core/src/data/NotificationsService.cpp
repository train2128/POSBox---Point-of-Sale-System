#include "NotificationsService.h"
#include "LocalDb.h"
#include "Prefixes.h"
#include <QCoreApplication>
#include <QDate>
#include <QHash>
#include <QJsonArray>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>

namespace {

struct Settings {
    bool openTicketEnabled = true;
    int openTicketDays = 7;
    bool outOfStockEnabled = true;
    bool nearlyOutOfStockEnabled = true;
    int lowStockMin = 3;
};

struct Derived {
    QString id;
    QString type;
    int contextId = 0;
    QString title;
    QString message;
    QString signature;
    QString action;    // "App|flag|value" deep link the bell follows
    QString priority;  // "high" | "normal"
};

Settings loadSettings(QSqlDatabase& db)
{
    Settings s;
    QSqlQuery q(db);
    if (!q.exec("SELECT open_ticket_enabled, open_ticket_days, out_of_stock_enabled, "
                "nearly_out_of_stock_enabled, low_stock_min FROM notification_settings WHERE id = 1")
        || !q.next())
        return s;
    s.openTicketEnabled = q.value(0).toInt() != 0;
    s.openTicketDays = q.value(1).toInt() > 0 ? q.value(1).toInt() : 7;
    s.outOfStockEnabled = q.value(2).toInt() != 0;
    s.nearlyOutOfStockEnabled = q.value(3).toInt() != 0;
    s.lowStockMin = q.value(4).isNull() ? 3 : q.value(4).toInt();
    return s;
}

QJsonObject settingsJson(const Settings& s)
{
    return QJsonObject{
        {"open_ticket_enabled", s.openTicketEnabled},
        {"open_ticket_days", s.openTicketDays},
        {"out_of_stock_enabled", s.outOfStockEnabled},
        {"nearly_out_of_stock_enabled", s.nearlyOutOfStockEnabled},
        {"low_stock_min", s.lowStockMin},
    };
}

// A grouped stock alert re-alerts when its member set changes.
QString signatureFor(const QSet<int>& memberIds)
{
    QStringList ids;
    for (int id : memberIds)
        ids << QString::number(id);
    ids.sort();
    return ids.join(',');
}

QList<Derived> derive(QSqlDatabase& db, const Settings& s)
{
    QList<Derived> list;
    const QString today = QDate::currentDate().toString(Qt::ISODate);

    if (s.openTicketEnabled) {
        QSqlQuery q(db);
        q.prepare("SELECT id, ticket_number, customer_name, total, deposit_amount, date "
                  "FROM tickets WHERE status = 'OPEN' "
                  "AND date(date, '+' || ? || ' days') <= ?");
        q.addBindValue(s.openTicketDays);
        q.addBindValue(today);
        if (q.exec()) {
            while (q.next()) {
                const int ticketId = q.value(0).toInt();
                const QString number = q.value(1).toString();
                const QString customer = q.value(2).toString();
                const double outstanding = qMax(0.0, q.value(3).toDouble() - q.value(4).toDouble());
                const QDate openDate = QDate::fromString(q.value(5).toString(), Qt::ISODate);
                const int daysOpen = openDate.isValid() ? openDate.daysTo(QDate::currentDate())
                                                        : s.openTicketDays;
                Derived d;
                d.id = QString("open_ticket:%1").arg(ticketId);
                d.type = "open_ticket";
                d.contextId = ticketId;
                d.title = QCoreApplication::translate("Notifications", "Unpaid ticket");
                d.message = QCoreApplication::translate("Notifications", "Ticket #%1 (%2) is open for %3 days - %4 to pay")
                                .arg(number,
                                     customer.isEmpty() ? QCoreApplication::translate("Notifications", "no customer") : customer,
                                     QString::number(daysOpen),
                                     AppHelpers::formatPrice(outstanding));
                d.signature = QString::number(ticketId);
                d.action = QStringLiteral("Register|open-ticket|") + QString::number(ticketId);
                d.priority = "normal";
                list.append(d);
            }
        }
    }

    // One effective-stock pass feeds both stock alerts.
    if (s.outOfStockEnabled || s.nearlyOutOfStockEnabled) {
        const int minStock = qBound(1, s.lowStockMin, 10);
        QSqlQuery q(db);
        q.exec("SELECT p.id, p.name, "
               "CASE WHEN p.has_variants = 1 "
               "THEN COALESCE((SELECT SUM(stock) FROM product_variants v WHERE v.parent_product_id = p.id), 0) "
               "ELSE p.stock_quantity END AS eff_stock "
               "FROM products p WHERE p.track_stock = 1 AND p.is_service = 0");

        QList<QPair<int, QString>> outItems;
        QList<QPair<int, QString>> lowItems;
        while (q.next()) {
            const int pid = q.value(0).toInt();
            const QString name = q.value(1).toString();
            const double stock = q.value(2).toDouble();
            if (s.outOfStockEnabled && stock == 0)
                outItems.append(qMakePair(pid, name));
            else if (s.nearlyOutOfStockEnabled && stock > 0 && stock <= minStock)
                lowItems.append(qMakePair(pid, name));
        }

        if (s.outOfStockEnabled && !outItems.isEmpty()) {
            Derived d;
            QSet<int> ids;
            for (const auto& it : outItems) ids.insert(it.first);
            d.id = "out_of_stock:0";
            d.type = "out_of_stock";
            d.title = QCoreApplication::translate("Notifications", "Out of stock");
            d.message = outItems.size() == 1
                ? QCoreApplication::translate("Notifications", "%1 is out of stock").arg(outItems.first().second)
                : QCoreApplication::translate("Notifications", "%1 products are out of stock").arg(outItems.size());
            d.signature = signatureFor(ids);
            d.action = QStringLiteral("Inventory|out|1");
            d.priority = "high";
            list.append(d);
        }
        if (s.nearlyOutOfStockEnabled && !lowItems.isEmpty()) {
            Derived d;
            QSet<int> ids;
            for (const auto& it : lowItems) ids.insert(it.first);
            d.id = "nearly_out_of_stock:0";
            d.type = "nearly_out_of_stock";
            d.title = QCoreApplication::translate("Notifications", "Low stock");
            d.message = lowItems.size() == 1
                ? QCoreApplication::translate("Notifications", "%1 is below minimum stock").arg(lowItems.first().second)
                : QCoreApplication::translate("Notifications", "%1 products are below minimum stock").arg(lowItems.size());
            d.signature = signatureFor(ids);
            d.action = QStringLiteral("Inventory|low|2");
            d.priority = "normal";
            list.append(d);
        }
    }
    return list;
}

bool reconcile(QSqlDatabase& db, const QList<Derived>& derived, QString* errorOut)
{
    if (!db.transaction()) {
        if (errorOut) *errorOut = "The database is busy, please try again";
        return false;
    }

    for (const Derived& d : derived) {
        QSqlQuery sel(db);
        sel.prepare("SELECT signature FROM notifications WHERE id = ?");
        sel.addBindValue(d.id);
        if (sel.exec() && sel.next()) {
            if (sel.value(0).toString() != d.signature) {
                // Members changed: everyone sees it again.
                QSqlQuery clr(db);
                clr.prepare("DELETE FROM notification_reads WHERE notification_id = ?");
                clr.addBindValue(d.id);
                clr.exec();
            }
            QSqlQuery upd(db);
            upd.prepare("UPDATE notifications SET title = ?, message = ?, signature = ?, "
                        "updated_at = CASE WHEN signature = ? THEN updated_at ELSE CURRENT_TIMESTAMP END "
                        "WHERE id = ?");
            upd.addBindValue(d.title);
            upd.addBindValue(d.message);
            upd.addBindValue(d.signature);
            upd.addBindValue(d.signature);
            upd.addBindValue(d.id);
            if (!upd.exec()) {
                db.rollback();
                if (errorOut) *errorOut = upd.lastError().text();
                return false;
            }
        } else {
            QSqlQuery ins(db);
            ins.prepare("INSERT INTO notifications (id, type, context_id, signature, title, message, "
                        "read, updated_at) VALUES (?, ?, ?, ?, ?, ?, 0, CURRENT_TIMESTAMP)");
            ins.addBindValue(d.id);
            ins.addBindValue(d.type);
            ins.addBindValue(d.contextId);
            ins.addBindValue(d.signature);
            ins.addBindValue(d.title);
            ins.addBindValue(d.message);
            if (!ins.exec()) {
                db.rollback();
                if (errorOut) *errorOut = ins.lastError().text();
                return false;
            }
        }
    }

    // Rows whose condition resolved disappear, with their read state.
    QSet<QString> live;
    for (const Derived& d : derived)
        live.insert(d.id);
    QStringList dead;
    QSqlQuery all(db);
    if (all.exec("SELECT id FROM notifications")) {
        while (all.next()) {
            const QString id = all.value(0).toString();
            if (!live.contains(id))
                dead << id;
        }
    }
    for (const QString& id : dead) {
        QSqlQuery dr(db);
        dr.prepare("DELETE FROM notification_reads WHERE notification_id = ?");
        dr.addBindValue(id);
        dr.exec();
        QSqlQuery dn(db);
        dn.prepare("DELETE FROM notifications WHERE id = ?");
        dn.addBindValue(id);
        dn.exec();
    }

    if (!db.commit()) {
        db.rollback();
        if (errorOut) *errorOut = "Commit failed";
        return false;
    }
    return true;
}

bool stamp(const QString& username, const QString& id, bool dismiss, QString* errorOut)
{
    QSqlDatabase db = LocalDb::db();
    QSqlQuery q(db);
    // INSERT OR IGNORE first so an existing row keeps its dismissal flag.
    if (id.isEmpty()) {
        q.prepare("INSERT OR IGNORE INTO notification_reads (username, notification_id) "
                  "SELECT ?, id FROM notifications");
        q.addBindValue(username);
    } else {
        q.prepare("INSERT OR IGNORE INTO notification_reads (username, notification_id) "
                  "SELECT ?, id FROM notifications WHERE id = ?");
        q.addBindValue(username);
        q.addBindValue(id);
    }
    if (!q.exec()) {
        if (errorOut) *errorOut = q.lastError().text();
        return false;
    }
    QSqlQuery u(db);
    const QString set = dismiss ? "dismissed = 1, read_at = CURRENT_TIMESTAMP"
                                : "read_at = CURRENT_TIMESTAMP";
    if (id.isEmpty()) {
        u.prepare("UPDATE notification_reads SET " + set + " WHERE username = ?");
        u.addBindValue(username);
    } else {
        u.prepare("UPDATE notification_reads SET " + set + " WHERE username = ? AND notification_id = ?");
        u.addBindValue(username);
        u.addBindValue(id);
    }
    if (!u.exec()) {
        if (errorOut) *errorOut = u.lastError().text();
        return false;
    }
    return true;
}

} // namespace

QJsonObject NotificationsService::sync(const QString& username, QString* errorOut)
{
    if (username.isEmpty()) {
        if (errorOut) *errorOut = "No user signed in";
        return {};
    }
    QSqlDatabase db = LocalDb::db();
    const Settings settings = loadSettings(db);
    const QList<Derived> derived = derive(db, settings);
    if (!reconcile(db, derived, errorOut))
        return {};

    QHash<QString, Derived> byId;
    for (const Derived& d : derived)
        byId.insert(d.id, d);

    QSqlQuery list(db);
    list.prepare("SELECT n.id, n.type, n.context_id, n.title, n.message, n.updated_at, "
                 "(r.read_at IS NOT NULL) AS is_read "
                 "FROM notifications n "
                 "LEFT JOIN notification_reads r ON r.notification_id = n.id AND r.username = ? "
                 "WHERE COALESCE(r.dismissed, 0) = 0 "
                 "ORDER BY n.updated_at DESC, n.id");
    list.addBindValue(username);
    QJsonArray rows;
    if (list.exec()) {
        while (list.next()) {
            const Derived d = byId.value(list.value(0).toString());
            rows.append(QJsonObject{
                {"id", list.value(0).toString()},
                {"type", list.value(1).toString()},
                {"context_id", list.value(2).toInt()},
                {"title", list.value(3).toString()},
                {"message", list.value(4).toString()},
                {"read", list.value(6).toInt() != 0},
                {"date", list.value(5).toString().left(10)},
                {"action", d.action},
                {"priority", d.priority.isEmpty() ? QStringLiteral("normal") : d.priority},
            });
        }
    }
    return QJsonObject{{"ok", true}, {"notifications", rows}, {"settings", settingsJson(settings)}};
}

QJsonObject NotificationsService::markRead(const QString& username, const QString& id, QString* errorOut)
{
    if (username.isEmpty() || id.isEmpty()) {
        if (errorOut) *errorOut = "Bad request";
        return {};
    }
    return stamp(username, id, false, errorOut) ? QJsonObject{{"ok", true}} : QJsonObject{};
}

QJsonObject NotificationsService::markAllRead(const QString& username, QString* errorOut)
{
    if (username.isEmpty()) {
        if (errorOut) *errorOut = "No user signed in";
        return {};
    }
    return stamp(username, QString(), false, errorOut) ? QJsonObject{{"ok", true}} : QJsonObject{};
}

QJsonObject NotificationsService::dismiss(const QString& username, const QString& id, QString* errorOut)
{
    if (username.isEmpty() || id.isEmpty()) {
        if (errorOut) *errorOut = "Bad request";
        return {};
    }
    return stamp(username, id, true, errorOut) ? QJsonObject{{"ok", true}} : QJsonObject{};
}

QJsonObject NotificationsService::dismissAll(const QString& username, QString* errorOut)
{
    if (username.isEmpty()) {
        if (errorOut) *errorOut = "No user signed in";
        return {};
    }
    return stamp(username, QString(), true, errorOut) ? QJsonObject{{"ok", true}} : QJsonObject{};
}
