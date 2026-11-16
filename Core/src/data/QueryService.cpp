#include "QueryService.h"
#include "LocalDb.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QVariant>
#include <QSet>
#include <QDebug>

namespace {

// Writable/readable tables and their client-settable columns.
// Primary keys and managed timestamps are never writable.
const QMap<QString, QSet<QString>>& schema()
{
    static const QMap<QString, QSet<QString>> s = {
        {"accounts", {"username", "password_hash", "role", "display_name", "email", "phone", "is_active"}},
        {"categories", {"name", "description", "display_order", "color", "icon_path", "visible_in_retail", "visible_in_cafe"}},
        {"products", {"name", "description", "cost", "price", "stock_quantity", "track_stock", "min_stock_alert", "barcode", "sku", "category_id", "is_service", "has_variants", "image_path", "out_of_stock_since"}},
        {"product_variants", {"parent_product_id", "variant_name", "cost", "price", "stock", "out_of_stock_since", "barcode", "sku", "is_active"}},
        {"tickets", {"ticket_number", "date", "time", "seller_username", "seller_display_name", "subtotal", "discount_amount", "tax_amount", "total", "payment_method", "status", "customer_name", "customer_phone", "deposit_amount", "description", "due_date"}},
        {"ticket_items", {"ticket_id", "product_id", "variant_id", "product_name", "variant_name", "category_name", "barcode", "quantity", "unit_price", "unit_cost", "total_price"}},
        {"discounts", {"name", "type", "value", "is_active", "start_date", "end_date"}},
        {"recurring_expenses", {"name", "is_active", "type", "amount", "recurrence_unit", "recurrence_value", "start_date", "next_due_date", "notes", "category"}},
        {"one_time_expenses", {"name", "amount", "date", "notes"}},
        {"recurring_expense_payments", {"expense_id", "amount", "paid_date"}},
        {"shop_profile", {"shop_name", "address", "postal", "city", "country", "phone", "email", "website", "facebook", "instagram", "tiktok", "whatsapp", "business_mode", "currency", "tax_rate", "logo_path", "receipt_design"}},
        {"registers", {"name", "description", "is_active", "printer_name", "cash_drawer_port"}},
        {"user_permissions", {"username", "permission_key", "is_allowed"}},
        {"notification_settings", {"open_ticket_enabled", "open_ticket_days", "expense_due_enabled", "expense_due_days", "expense_overdue_enabled", "out_of_stock_enabled", "nearly_out_of_stock_enabled", "low_stock_min"}},
    };
    return s;
}

QVariant jsonToSql(const QJsonValue& v)
{
    if (v.isNull() || v.isUndefined())
        return QVariant();
    if (v.isDouble())
        return v.toDouble();
    if (v.isBool())
        return v.toBool() ? 1 : 0;
    return v.toString();
}

QJsonValue sqlToJson(const QVariant& v)
{
    if (v.isNull() || !v.isValid())
        return QJsonValue();
    if (v.typeId() == QMetaType::Bool)
        return QJsonValue(v.toBool());
    if (v.typeId() == QMetaType::Double || v.typeId() == QMetaType::Float
        || v.typeId() == QMetaType::Int || v.typeId() == QMetaType::LongLong
        || v.typeId() == QMetaType::UInt || v.typeId() == QMetaType::ULongLong)
        return QJsonValue(v.toDouble());
    return QJsonValue(v.toString());
}

// Guards that no whitelist can express.
bool guardDelete(const QString& table, const QJsonObject& where, QString* errorOut)
{
    if (table == "accounts") {
        QString target = where.value("username").toString();
        if (target == "admin") {
            if (errorOut) *errorOut = "The built-in admin account cannot be deleted";
            return false;
        }
        QSqlQuery q(LocalDb::db());
        q.prepare("SELECT COUNT(*) FROM accounts WHERE role = 'Admin' AND is_active = 1"
                  " AND username != ?");
        q.addBindValue(target.isEmpty() ? QString() : target);
        if (target.isEmpty() || (q.exec() && q.next() && q.value(0).toInt() == 0)) {
            if (target.isEmpty()) {
                if (errorOut) *errorOut = "Refusing blind delete on accounts";
                return false;
            }
            QSqlQuery r(LocalDb::db());
            r.prepare("SELECT role FROM accounts WHERE username = ?");
            r.addBindValue(target);
            if (r.exec() && r.next() && r.value(0).toString() == "Admin") {
                if (errorOut) *errorOut = "Cannot delete the last active admin";
                return false;
            }
        }
    }
    if (where.isEmpty()) {
        if (errorOut) *errorOut = "Refusing blind delete";
        return false;
    }
    return true;
}

bool guardUpdate(const QString& table, const QJsonObject& values, const QJsonObject& where, QString* errorOut)
{
    if (table == "accounts" && values.contains("username")) {
        if (errorOut) *errorOut = "Usernames cannot be renamed";
        return false;
    }
    if (where.isEmpty()) {
        if (errorOut) *errorOut = "Refusing blind update";
        return false;
    }
    return true;
}

} // namespace

bool QueryService::isTable(const QString& table)
{
    return schema().contains(table);
}

namespace {

QJsonArray rowsToJson(QSqlQuery& q)
{
    QJsonArray rows;
    while (q.next()) {
        QJsonObject row;
        QSqlRecord r = q.record();
        for (int i = 0; i < r.count(); ++i)
            row[r.fieldName(i)] = sqlToJson(q.value(i));
        rows.append(row);
    }
    return rows;
}

} // namespace

QJsonObject QueryService::named(const QJsonObject& req, QString* errorOut)
{
    QString name = req.value("name").toString();
    QSqlDatabase db = LocalDb::db();
    QSqlQuery q(db);

    if (name == "product_list") {
        // Mirrors the legacy PRODUCT_SELECT_ALL incl. computed stock.
        q.prepare(
            "SELECT p.id, p.name, p.description, p.cost, p.price, p.stock_quantity, "
            "p.track_stock, p.min_stock_alert, p.barcode, p.sku, p.category_id, "
            "c.name as category_name, p.is_service, p.has_variants, p.image_path, "
            "p.created_at, p.updated_at, "
            "COALESCE(CASE WHEN p.has_variants = 1 THEN "
            "(SELECT SUM(stock) FROM product_variants WHERE parent_product_id = p.id AND is_active = 1) "
            "ELSE p.stock_quantity END, 0) as stock "
            "FROM products p LEFT JOIN categories c ON p.category_id = c.id "
            "ORDER BY p.name");
    } else if (name == "discounts_active") {
        q.prepare(
            "SELECT id, name, type, value FROM discounts WHERE is_active = 1 "
            "AND (start_date IS NULL OR start_date <= date('now')) "
            "AND (end_date IS NULL OR end_date >= date('now'))");
    } else if (name == "income_by_date") {
        q.prepare("SELECT COALESCE(SUM(total), 0) FROM tickets WHERE status = 'COMPLETED' AND date = ?");
        q.addBindValue(req.value("date").toString());
    } else if (name == "income_by_month") {
        q.prepare("SELECT COALESCE(SUM(total), 0) AS total, COUNT(*) AS count FROM tickets "
                  "WHERE status = 'COMPLETED' AND strftime('%Y', date) = ? AND strftime('%m', date) = ?");
        q.addBindValue(req.value("year").toString());
        q.addBindValue(req.value("month").toString());
    } else if (name == "income_by_year") {
        q.prepare("SELECT strftime('%m', date) AS month, COALESCE(SUM(total), 0) AS total, "
                  "COUNT(*) AS count FROM tickets "
                  "WHERE status = 'COMPLETED' AND strftime('%Y', date) = ? "
                  "GROUP BY month ORDER BY month");
        q.addBindValue(req.value("year").toString());
    } else if (name == "ticket_cost_by_month") {
        q.prepare("SELECT COALESCE(SUM(ti.unit_cost * ti.quantity), 0) AS cost "
                  "FROM ticket_items ti JOIN tickets t ON ti.ticket_id = t.id "
                  "WHERE t.status = 'COMPLETED' AND strftime('%Y', t.date) = ? AND strftime('%m', t.date) = ?");
        q.addBindValue(req.value("year").toString());
        q.addBindValue(req.value("month").toString());
    } else if (name == "top_products_by_month") {
        q.prepare("SELECT ti.product_name, SUM(ti.quantity) AS total_qty "
                  "FROM ticket_items ti JOIN tickets t ON ti.ticket_id = t.id "
                  "WHERE t.status = 'COMPLETED' AND strftime('%Y', t.date) = ? AND strftime('%m', t.date) = ? "
                  "GROUP BY ti.product_name ORDER BY total_qty DESC LIMIT 3");
        q.addBindValue(req.value("year").toString());
        q.addBindValue(req.value("month").toString());
    } else if (name == "expenses_by_month") {
        q.prepare("SELECT id, name, amount, date, notes FROM one_time_expenses "
                  "WHERE strftime('%Y', date) = ? AND strftime('%m', date) = ? ORDER BY date DESC");
        q.addBindValue(req.value("year").toString());
        q.addBindValue(req.value("month").toString());
    } else if (name == "stock_check_product") {
        q.prepare("SELECT is_service, stock_quantity FROM products WHERE id = ?");
        q.addBindValue(req.value("id").toInt());
    } else if (name == "stock_check_variant") {
        q.prepare(
            "SELECT pv.stock, p.is_service FROM product_variants pv "
            "JOIN products p ON pv.parent_product_id = p.id WHERE pv.id = ?");
        q.addBindValue(req.value("id").toInt());
    } else {
        if (errorOut) *errorOut = "Unknown query: " + name;
        return {};
    }

    if (!q.exec()) {
        if (errorOut) *errorOut = q.lastError().text();
        return {};
    }
    return QJsonObject{{"ok", true}, {"rows", rowsToJson(q)}};
}

static bool roleDefault(const QString& action, const QString& role)
{
    // Mirrors AccessControl::getRoleDefaultPermission.
    if (role == "Admin")
        return true;
    if (role == "Manager")
        return action == "add_category" || action == "manage_discounts";
    if (role == "Cashier")
        return action == "withdraw_cash" || action == "delete_ticket"
            || action == "delete_product";
    return false;
}

QJsonObject QueryService::permission(const QString& username, const QString& permissionKey)
{
    if (username.isEmpty())
        return QJsonObject{{"ok", true}, {"allowed", false}};
    QSqlDatabase db = LocalDb::db();
    QSqlQuery q(db);
    q.prepare("SELECT role FROM accounts WHERE username = ?");
    q.addBindValue(username);
    if (!q.exec() || !q.next())
        return QJsonObject{{"ok", true}, {"allowed", false}};
    QString role = q.value(0).toString();
    if (role == "Admin")
        return QJsonObject{{"ok", true}, {"allowed", true}};
    QSqlQuery o(db);
    o.prepare("SELECT is_allowed FROM user_permissions WHERE username = ? AND permission_key = ?");
    o.addBindValue(username);
    o.addBindValue(permissionKey);
    if (o.exec() && o.next()) {
        int v = o.value(0).toInt();
        if (v == 1) return QJsonObject{{"ok", true}, {"allowed", true}};
        if (v == 0) return QJsonObject{{"ok", true}, {"allowed", false}};
    }
    return QJsonObject{{"ok", true}, {"allowed", roleDefault(permissionKey, role)}};
}

QJsonObject QueryService::select(const QJsonObject& req, QString* errorOut)
{
    QString table = req.value("table").toString();
    if (!isTable(table)) {
        if (errorOut) *errorOut = "Unknown table";
        return {};
    }
    QSqlDatabase db = LocalDb::db();
    QSqlRecord rec = db.record(table);
    QStringList cols;
    QJsonArray want = req.value("columns").toArray();
    if (want.isEmpty()) {
        for (int i = 0; i < rec.count(); ++i)
            cols << rec.fieldName(i);
    } else {
        for (const QJsonValue& v : want) {
            QString c = v.toString();
            if (rec.indexOf(c) < 0) {
                if (errorOut) *errorOut = "Unknown column: " + c;
                return {};
            }
            cols << c;
        }
    }
    // password hashes are never handed to the windows.
    cols.removeAll("password_hash");

    QString sql = QString("SELECT %1 FROM %2").arg(cols.join(","), table);
    QJsonObject where = req.value("where").toObject();
    QList<QVariant> binds;
    if (!where.isEmpty()) {
        QStringList conds;
        for (auto it = where.constBegin(); it != where.constEnd(); ++it) {
            if (rec.indexOf(it.key()) < 0) {
                if (errorOut) *errorOut = "Unknown column: " + it.key();
                return {};
            }
            if (it.value().isNull() || it.value().isUndefined())
                conds << (it.key() + " IS NULL");
            else {
                conds << (it.key() + " = ?");
                binds << jsonToSql(it.value());
            }
        }
        sql += " WHERE " + conds.join(" AND ");
    }
    QString order = req.value("order").toString().trimmed();
    if (!order.isEmpty()) {
        // "col [ASC|DESC], ..." with identifier validation only.
        QStringList parts;
        for (const QString& p : order.split(',', Qt::SkipEmptyParts)) {
            QStringList w = p.trimmed().split(' ', Qt::SkipEmptyParts);
            if (w.isEmpty() || w.size() > 2 || rec.indexOf(w[0]) < 0
                || (w.size() == 2 && w[1].toUpper() != "ASC" && w[1].toUpper() != "DESC")) {
                if (errorOut) *errorOut = "Bad order clause";
                return {};
            }
            parts << p.trimmed();
        }
        sql += " ORDER BY " + parts.join(", ");
    }
    int limit = req.value("limit").toInt(0);
    if (limit > 0)
        sql += QString(" LIMIT %1").arg(qBound(1, limit, 5000));

    QSqlQuery q(db);
    q.prepare(sql);
    for (const QVariant& b : binds) q.addBindValue(b);
    if (!q.exec()) {
        if (errorOut) *errorOut = q.lastError().text();
        return {};
    }
    QJsonArray rows;
    while (q.next()) {
        QJsonObject row;
        for (const QString& c : cols)
            row[c] = sqlToJson(q.value(c));
        rows.append(row);
    }
    return QJsonObject{{"ok", true}, {"rows", rows}};
}

QJsonObject QueryService::insert(const QJsonObject& req, QString* errorOut){
    QString table = req.value("table").toString();
    if (!isTable(table)) {
        if (errorOut) *errorOut = "Unknown table";
        return {};
    }
    QJsonObject values = req.value("values").toObject();
    if (values.isEmpty()) {
        if (errorOut) *errorOut = "No values";
        return {};
    }
    const QSet<QString>& allowed = schema()[table];
    QStringList cols;
    for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        if (!allowed.contains(it.key())) {
            if (errorOut) *errorOut = "Column not writable: " + it.key();
            return {};
        }
        cols << it.key();
    }
    QStringList holders;
    for (int i = 0; i < cols.size(); ++i) holders << "?";
    QSqlQuery q(LocalDb::db());
    q.prepare(QString("INSERT INTO %1 (%2) VALUES (%3)").arg(table, cols.join(","), holders.join(",")));
    for (const QString& c : cols) q.addBindValue(jsonToSql(values.value(c)));
    if (!q.exec()) {
        if (errorOut) *errorOut = q.lastError().text();
        return {};
    }
    return QJsonObject{{"ok", true}, {"id", q.lastInsertId().toDouble()}};
}

QJsonObject QueryService::update(const QJsonObject& req, QString* errorOut)
{
    QString table = req.value("table").toString();
    if (!isTable(table)) {
        if (errorOut) *errorOut = "Unknown table";
        return {};
    }
    QSqlDatabase db = LocalDb::db();
    QSqlRecord rec = db.record(table);
    QJsonObject values = req.value("values").toObject();
    QJsonObject where = req.value("where").toObject();
    if (values.isEmpty()) {
        if (errorOut) *errorOut = "No values";
        return {};
    }
    if (!guardUpdate(table, values, where, errorOut))
        return {};
    const QSet<QString>& allowed = schema()[table];
    QStringList sets;
    QList<QVariant> binds;
    for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        if (!allowed.contains(it.key())) {
            if (errorOut) *errorOut = "Column not writable: " + it.key();
            return {};
        }
        sets << (it.key() + " = ?");
        binds << jsonToSql(it.value());
    }
    QStringList conds;
    for (auto it = where.constBegin(); it != where.constEnd(); ++it) {
        if (rec.indexOf(it.key()) < 0) {
            if (errorOut) *errorOut = "Unknown column: " + it.key();
            return {};
        }
        if (it.value().isNull() || it.value().isUndefined())
            conds << (it.key() + " IS NULL");
        else {
            conds << (it.key() + " = ?");
            binds << jsonToSql(it.value());
        }
    }
    QSqlQuery q(db);
    q.prepare(QString("UPDATE %1 SET %2 WHERE %3").arg(table, sets.join(","), conds.join(" AND ")));
    for (const QVariant& b : binds) q.addBindValue(b);
    if (!q.exec()) {
        if (errorOut) *errorOut = q.lastError().text();
        return {};
    }
    return QJsonObject{{"ok", true}, {"affected", q.numRowsAffected()}};
}

QJsonObject QueryService::remove(const QJsonObject& req, QString* errorOut)
{
    QString table = req.value("table").toString();
    if (!isTable(table)) {
        if (errorOut) *errorOut = "Unknown table";
        return {};
    }
    QSqlDatabase db = LocalDb::db();
    QSqlRecord rec = db.record(table);
    QJsonObject where = req.value("where").toObject();
    if (!guardDelete(table, where, errorOut))
        return {};
    QStringList conds;
    QList<QVariant> binds;
    for (auto it = where.constBegin(); it != where.constEnd(); ++it) {
        if (rec.indexOf(it.key()) < 0) {
            if (errorOut) *errorOut = "Unknown column: " + it.key();
            return {};
        }
        if (it.value().isNull() || it.value().isUndefined())
            conds << (it.key() + " IS NULL");
        else {
            conds << (it.key() + " = ?");
            binds << jsonToSql(it.value());
        }
    }
    QSqlQuery q(db);
    q.prepare(QString("DELETE FROM %1 WHERE %2").arg(table, conds.join(" AND ")));
    for (const QVariant& b : binds) q.addBindValue(b);
    if (!q.exec()) {
        if (errorOut) *errorOut = q.lastError().text();
        return {};
    }
    return QJsonObject{{"ok", true}, {"affected", q.numRowsAffected()}};
}
