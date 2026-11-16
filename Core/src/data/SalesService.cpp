#include "SalesService.h"
#include <QDate>
#include "LocalDb.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDebug>

namespace {

QVariant jv(const QJsonObject& o, const QString& key)
{
    QJsonValue v = o.value(key);
    if (v.isNull() || v.isUndefined())
        return QVariant();
    if (v.isDouble())
        return v.toDouble();
    if (v.isBool())
        return v.toBool() ? 1 : 0;
    return v.toString();
}

// Mirror of the client out_of_stock_since maintenance.
bool refreshProductFlag(QSqlDatabase& db, int productId)
{
    QSqlQuery info(db);
    info.prepare("SELECT has_variants FROM products WHERE id = ?");
    info.addBindValue(productId);
    if (!info.exec() || !info.next())
        return false;
    bool hasVariants = info.value(0).toBool();
    bool allOut = true;
    if (hasVariants) {
        QSqlQuery v(db);
        v.prepare("SELECT stock FROM product_variants WHERE parent_product_id = ? AND is_active = 1");
        v.addBindValue(productId);
        if (!v.exec())
            return false;
        bool any = false;
        while (v.next()) {
            any = true;
            if (v.value(0).toInt() > 0) { allOut = false; break; }
        }
        if (!any)
            allOut = true;
    } else {
        QSqlQuery s(db);
        s.prepare("SELECT stock_quantity FROM products WHERE id = ?");
        s.addBindValue(productId);
        if (!s.exec() || !s.next())
            return false;
        allOut = (s.value(0).toInt() <= 0);
    }
    QSqlQuery u(db);
    if (allOut)
        u.prepare("UPDATE products SET out_of_stock_since = CURRENT_TIMESTAMP WHERE id = ? AND out_of_stock_since IS NULL");
    else
        u.prepare("UPDATE products SET out_of_stock_since = NULL WHERE id = ?");
    u.addBindValue(productId);
    return u.exec();
}

bool refreshVariantFlag(QSqlDatabase& db, int variantId)
{
    QSqlQuery info(db);
    info.prepare("SELECT parent_product_id, stock FROM product_variants WHERE id = ?");
    info.addBindValue(variantId);
    if (!info.exec() || !info.next())
        return false;
    int parentId = info.value(0).toInt();
    int stock = info.value(1).toInt();
    QSqlQuery u(db);
    if (stock <= 0)
        u.prepare("UPDATE product_variants SET out_of_stock_since = CURRENT_TIMESTAMP WHERE id = ? AND out_of_stock_since IS NULL");
    else
        u.prepare("UPDATE product_variants SET out_of_stock_since = NULL WHERE id = ?");
    u.addBindValue(variantId);
    if (!u.exec())
        return false;
    return refreshProductFlag(db, parentId);
}

bool applyDelta(QSqlDatabase& db, bool isVariant, int id, double delta, double* stockOut)
{
    QSqlQuery q(db);
    if (isVariant)
        q.prepare("UPDATE product_variants SET stock = stock + ? WHERE id = ?");
    else
        q.prepare("UPDATE products SET stock_quantity = stock_quantity + ? WHERE id = ?");
    q.addBindValue(delta);
    q.addBindValue(id);
    if (!q.exec() || q.numRowsAffected() <= 0)
        return false;
    if (isVariant) {
        if (!refreshVariantFlag(db, id))
            return false;
        QSqlQuery s(db);
        s.prepare("SELECT stock FROM product_variants WHERE id = ?");
        s.addBindValue(id);
        if (s.exec() && s.next() && stockOut) *stockOut = s.value(0).toDouble();
    } else {
        if (!refreshProductFlag(db, id))
            return false;
        QSqlQuery s(db);
        s.prepare("SELECT stock_quantity FROM products WHERE id = ?");
        s.addBindValue(id);
        if (s.exec() && s.next() && stockOut) *stockOut = s.value(0).toDouble();
    }
    return true;
}

bool insertItems(QSqlDatabase& db, int ticketId, const QJsonArray& items, QString* errorOut)
{
    static const QStringList cols = {"product_id", "variant_id", "product_name", "variant_name",
                                     "category_name", "barcode", "quantity", "unit_price",
                                     "unit_cost", "total_price"};
    for (const QJsonValue& iv : items) {
        QJsonObject it = iv.toObject();
        QStringList holders;
        for (int i = 0; i < cols.size(); ++i) holders << "?";
        QSqlQuery q(db);
        q.prepare(QString("INSERT INTO ticket_items (ticket_id, %1) VALUES (?, %2)")
                      .arg(cols.join(","), holders.join(",")));
        q.addBindValue(ticketId);
        for (const QString& c : cols) q.addBindValue(jv(it, c));
        if (!q.exec()) {
            if (errorOut) *errorOut = q.lastError().text();
            return false;
        }
    }
    return true;
}

// Unique ticket number with deterministic cross-terminal fallback.
QString claimNumber(QSqlDatabase& db, const QString& wanted)
{
    if (wanted.isEmpty())
        return QString();
    QSqlQuery q(db);
    q.prepare("SELECT id FROM tickets WHERE ticket_number = ?");
    q.addBindValue(wanted);
    if (!q.exec() || !q.next())
        return wanted;
    for (int n = 2; n < 1000; ++n) {
        QString cand = wanted + "-" + QString::number(n);
        q.prepare("SELECT id FROM tickets WHERE ticket_number = ?");
        q.addBindValue(cand);
        if (q.exec() && !q.next())
            return cand;
    }
    return QString();
}

} // namespace

QJsonObject SalesService::adjustStock(bool isVariant, int id, double deltaOrValue,
                                      bool absolute, QString* errorOut)
{
    if (id <= 0) {
        if (errorOut) *errorOut = "Bad stock adjustment";
        return {};
    }
    QSqlDatabase db = LocalDb::db();
    double stock = 0;

    if (absolute) {
        // Absolute set: read the current value and convert to a delta inside
        // one transaction so flag maintenance stays identical to the delta path.
        if (!db.transaction()) {
            if (errorOut) *errorOut = "The database is busy, please try again";
            return {};
        }
        QSqlQuery cur(db);
        cur.prepare(isVariant ? "SELECT stock FROM product_variants WHERE id = ?"
                              : "SELECT stock_quantity FROM products WHERE id = ?");
        cur.addBindValue(id);
        if (!cur.exec() || !cur.next()) {
            db.rollback();
            if (errorOut) *errorOut = "Stock item not found";
            return {};
        }
        const double current = cur.value(0).toDouble();
        const double delta = deltaOrValue - current;
        if (delta == 0) {
            if (!db.commit()) {
                db.rollback();
                if (errorOut) *errorOut = "Commit failed";
                return {};
            }
            return QJsonObject{{"ok", true}, {"stock", current}};
        }
        if (!applyDelta(db, isVariant, id, delta, &stock)) {
            db.rollback();
            if (errorOut) *errorOut = "Stock adjustment failed";
            return {};
        }
        if (!db.commit()) {
            db.rollback();
            if (errorOut) *errorOut = "Commit failed";
            return {};
        }
        return QJsonObject{{"ok", true}, {"stock", stock}};
    }

    if (deltaOrValue == 0) {
        if (errorOut) *errorOut = "Bad stock adjustment";
        return {};
    }
    if (!applyDelta(db, isVariant, id, deltaOrValue, &stock)) {
        if (errorOut) *errorOut = "Stock adjustment failed";
        return {};
    }
    return QJsonObject{{"ok", true}, {"stock", stock}};
}

QJsonObject SalesService::productSave(const QJsonObject& req, QString* errorOut)
{
    QJsonObject p = req.value("product").toObject();
    if (p.isEmpty()) {
        if (errorOut) *errorOut = "Empty product";
        return {};
    }
    const QJsonArray variants  = req.value("variants").toArray();
    const QJsonArray deleteIds = req.value("delete_variant_ids").toArray();

    QSqlDatabase db = LocalDb::db();
    if (!db.transaction()) {
        if (errorOut) *errorOut = "The database is busy, please try again";
        return {};
    }

    const QStringList cols = {"name", "description", "cost", "price", "stock_quantity",
                              "track_stock", "min_stock_alert", "barcode", "sku",
                              "category_id", "is_service", "has_variants", "image_path"};
    int productId = req.value("id").toInt();
    if (productId > 0) {
        QStringList sets;
        for (const QString& c : cols) sets << (c + " = ?");
        QSqlQuery q(db);
        q.prepare(QString("UPDATE products SET %1, updated_at = CURRENT_TIMESTAMP WHERE id = ?")
                      .arg(sets.join(", ")));
        for (const QString& c : cols) q.addBindValue(jv(p, c));
        q.addBindValue(productId);
        if (!q.exec() || q.numRowsAffected() <= 0) {
            db.rollback();
            if (errorOut) *errorOut = "Product not found";
            return {};
        }
    } else {
        QStringList holders;
        for (int i = 0; i < cols.size(); ++i) holders << "?";
        QSqlQuery q(db);
        q.prepare(QString("INSERT INTO products (%1) VALUES (%2)")
                      .arg(cols.join(", "), holders.join(", ")));
        for (const QString& c : cols) q.addBindValue(jv(p, c));
        if (!q.exec()) {
            db.rollback();
            if (errorOut) *errorOut = q.lastError().text();
            return {};
        }
        productId = q.lastInsertId().toInt();
    }

    for (const QJsonValue& dv : deleteIds) {
        QSqlQuery d(db);
        d.prepare("DELETE FROM product_variants WHERE id = ? AND parent_product_id = ?");
        d.addBindValue(dv.toInt());
        d.addBindValue(productId);
        if (!d.exec()) {
            db.rollback();
            if (errorOut) *errorOut = d.lastError().text();
            return {};
        }
    }

    const QStringList vcols = {"variant_name", "cost", "price", "stock", "barcode", "sku", "is_active"};
    for (const QJsonValue& vv : variants) {
        QJsonObject v = vv.toObject();
        const int variantId = v.value("id").toInt();
        if (variantId > 0) {
            QStringList sets;
            for (const QString& c : vcols) sets << (c + " = ?");
            QSqlQuery q(db);
            q.prepare(QString("UPDATE product_variants SET %1 WHERE id = ? AND parent_product_id = ?")
                          .arg(sets.join(", ")));
            for (const QString& c : vcols) q.addBindValue(jv(v, c));
            q.addBindValue(variantId);
            q.addBindValue(productId);
            if (!q.exec()) {
                db.rollback();
                if (errorOut) *errorOut = q.lastError().text();
                return {};
            }
        } else {
            QSqlQuery q(db);
            q.prepare("INSERT INTO product_variants (parent_product_id, variant_name, cost, price, "
                      "stock, barcode, sku, is_active) VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
            q.addBindValue(productId);
            for (const QString& c : vcols) q.addBindValue(jv(v, c));
            if (!q.exec()) {
                db.rollback();
                if (errorOut) *errorOut = q.lastError().text();
                return {};
            }
        }
    }

    // Out-of-stock flag follows the saved state; services and untracked
    // products never carry the flag.
    const bool isService = p.value("is_service").toBool(false);
    const bool trackStock = !p.contains("track_stock") || p.value("track_stock").toBool(true);
    if (isService || !trackStock) {
        QSqlQuery clear(db);
        clear.prepare("UPDATE products SET out_of_stock_since = NULL WHERE id = ?");
        clear.addBindValue(productId);
        if (!clear.exec()) {
            db.rollback();
            if (errorOut) *errorOut = clear.lastError().text();
            return {};
        }
    } else if (!refreshProductFlag(db, productId)) {
        db.rollback();
        if (errorOut) *errorOut = "Stock flag update failed";
        return {};
    }

    if (!db.commit()) {
        db.rollback();
        if (errorOut) *errorOut = "Commit failed";
        return {};
    }
    return QJsonObject{{"ok", true}, {"id", productId}};
}

namespace {

// Restores every stock-tracked item of a ticket (variant or product path),
// mirroring the sale movement rules: services never moved stock.
bool restoreTicketStock(QSqlDatabase& db, int ticketId)
{
    QSqlQuery items(db);
    items.prepare("SELECT product_id, variant_id, quantity FROM ticket_items WHERE ticket_id = ?");
    items.addBindValue(ticketId);
    if (!items.exec())
        return false;

    struct Line { int productId; int variantId; double qty; };
    QList<Line> lines;
    while (items.next())
        lines << Line{items.value(0).toInt(), items.value(1).toInt(), items.value(2).toDouble()};

    for (const Line& l : lines) {
        if (l.qty <= 0)
            continue;
        if (l.variantId > 0) {
            QSqlQuery u(db);
            u.prepare("UPDATE product_variants SET stock = stock + ? WHERE id = ?");
            u.addBindValue(l.qty);
            u.addBindValue(l.variantId);
            if (!u.exec() || !refreshVariantFlag(db, l.variantId))
                return false;
        } else if (l.productId > 0) {
            QSqlQuery chk(db);
            chk.prepare("SELECT is_service FROM products WHERE id = ?");
            chk.addBindValue(l.productId);
            if (chk.exec() && chk.next() && chk.value(0).toBool())
                continue;
            QSqlQuery u(db);
            u.prepare("UPDATE products SET stock_quantity = stock_quantity + ? WHERE id = ?");
            u.addBindValue(l.qty);
            u.addBindValue(l.productId);
            if (!u.exec() || !refreshProductFlag(db, l.productId))
                return false;
        }
    }
    return true;
}

} // namespace

QJsonObject SalesService::refundTicket(int ticketId, QString* errorOut)
{
    if (ticketId <= 0) {
        if (errorOut) *errorOut = "Bad ticket id";
        return {};
    }
    QSqlDatabase db = LocalDb::db();
    if (!db.transaction()) {
        if (errorOut) *errorOut = "The database is busy, please try again";
        return {};
    }
    QSqlQuery chk(db);
    chk.prepare("SELECT status FROM tickets WHERE id = ?");
    chk.addBindValue(ticketId);
    if (!chk.exec() || !chk.next()) {
        db.rollback();
        if (errorOut) *errorOut = "Ticket not found";
        return {};
    }
    const QString status = chk.value(0).toString();
    if (status == "REFUNDED") {
        db.rollback();
        if (errorOut) *errorOut = "This ticket is already refunded";
        return {};
    }
    if (status != "COMPLETED") {
        db.rollback();
        if (errorOut) *errorOut = "Only completed tickets can be refunded";
        return {};
    }

    QSqlQuery upd(db);
    upd.prepare("UPDATE tickets SET status = 'REFUNDED' WHERE id = ?");
    upd.addBindValue(ticketId);
    if (!upd.exec() || upd.numRowsAffected() <= 0) {
        db.rollback();
        if (errorOut) *errorOut = "Refund update failed";
        return {};
    }
    if (!restoreTicketStock(db, ticketId)) {
        db.rollback();
        if (errorOut) *errorOut = "Stock restore failed";
        return {};
    }
    if (!db.commit()) {
        db.rollback();
        if (errorOut) *errorOut = "Commit failed";
        return {};
    }
    return QJsonObject{{"ok", true}, {"status", "REFUNDED"}};
}

QJsonObject SalesService::deleteTicket(int ticketId, QString* errorOut)
{
    if (ticketId <= 0) {
        if (errorOut) *errorOut = "Bad ticket id";
        return {};
    }
    QSqlDatabase db = LocalDb::db();
    if (!db.transaction()) {
        if (errorOut) *errorOut = "The database is busy, please try again";
        return {};
    }
    QSqlQuery chk(db);
    chk.prepare("SELECT status FROM tickets WHERE id = ?");
    chk.addBindValue(ticketId);
    if (!chk.exec() || !chk.next()) {
        db.rollback();
        if (errorOut) *errorOut = "Ticket not found";
        return {};
    }
    // Stock comes back only while the ticket is still COMPLETED: a REFUNDED
    // ticket was already restocked, and OPEN tickets never moved stock.
    if (chk.value(0).toString() == "COMPLETED" && !restoreTicketStock(db, ticketId)) {
        db.rollback();
        if (errorOut) *errorOut = "Stock restore failed";
        return {};
    }
    QSqlQuery del(db);
    del.prepare("DELETE FROM tickets WHERE id = ?");
    del.addBindValue(ticketId);
    if (!del.exec() || del.numRowsAffected() <= 0) {
        db.rollback();
        if (errorOut) *errorOut = "Delete failed";
        return {};
    }
    if (!db.commit()) {
        db.rollback();
        if (errorOut) *errorOut = "Commit failed";
        return {};
    }
    return QJsonObject{{"ok", true}};
}

QJsonObject SalesService::complete(const QJsonObject& req, QString* errorOut)
{
    QJsonObject t = req.value("ticket").toObject();
    QJsonArray items = req.value("items").toArray();
    if (t.isEmpty() || items.isEmpty()) {
        if (errorOut) *errorOut = "Empty sale";
        return {};
    }
    QSqlDatabase db = LocalDb::db();
    if (!db.transaction()) {
        if (errorOut) *errorOut = "The database is busy, please try again";
        return {};
    }

    QString number = claimNumber(db, t.value("ticket_number").toString());
    if (number.isEmpty()) {
        db.rollback();
        if (errorOut) *errorOut = "Ticket number collision";
        return {};
    }

    static const QStringList cols = {"ticket_number", "date", "time", "seller_username",
        "seller_display_name", "subtotal", "discount_amount", "tax_amount", "total",
        "payment_method", "status", "customer_name", "customer_phone", "deposit_amount",
        "description", "due_date"};
    QStringList holders;
    for (int i = 0; i < cols.size(); ++i) holders << "?";
    QSqlQuery ins(db);
    ins.prepare(QString("INSERT INTO tickets (%1) VALUES (%2)").arg(cols.join(","), holders.join(",")));
    for (const QString& c : cols)
        ins.addBindValue(c == "ticket_number" ? QVariant(number) : jv(t, c));
    if (!ins.exec()) {
        db.rollback();
        if (errorOut) *errorOut = ins.lastError().text();
        return {};
    }
    int ticketId = ins.lastInsertId().toInt();
    if (!insertItems(db, ticketId, items, errorOut)) {
        db.rollback();
        return {};
    }

    // Stock movements with service/custom skips, mirroring the client rules.
    // Each item's stock is verified against current values inside the same
    // transaction that applies its delta: no sale can drive stock below zero
    // (two registers racing on the last unit → the slower one is refused).
    for (const QJsonValue& iv : items) {
        QJsonObject it = iv.toObject();
        bool isService = it.value("is_service").toBool(false);
        if (isService)
            continue;
        int variantId = it.value("variant_id").toInt();
        int productId = it.value("product_id").toInt();
        double qty = it.value("quantity").toDouble();
        if (qty <= 0)
            continue;
        if (variantId > 0) {
            QSqlQuery chk(db);
            chk.prepare("SELECT pv.stock, p.is_service FROM product_variants pv "
                        "JOIN products p ON pv.parent_product_id = p.id WHERE pv.id = ?");
            chk.addBindValue(variantId);
            if (!chk.exec() || !chk.next()) {
                db.rollback();
                if (errorOut) *errorOut = "Variant not found";
                return {};
            }
            if (!chk.value(1).toBool() && chk.value(0).toDouble() < qty) {
                db.rollback();
                if (errorOut)
                    *errorOut = QString("Insufficient stock for %1 (%2 available)")
                                    .arg(it.value("product_name").toString())
                                    .arg(chk.value(0).toDouble());
                return {};
            }
            if (!applyDelta(db, true, variantId, -qty, nullptr)) {
                db.rollback();
                if (errorOut) *errorOut = "Stock update failed";
                return {};
            }
        } else if (productId > 0) {
            QSqlQuery chk(db);
            chk.prepare("SELECT is_service, stock_quantity FROM products WHERE id = ?");
            chk.addBindValue(productId);
            if (!chk.exec() || !chk.next()) {
                db.rollback();
                if (errorOut) *errorOut = "Product not found";
                return {};
            }
            if (chk.value(0).toBool())
                continue;
            if (chk.value(1).toDouble() < qty) {
                db.rollback();
                if (errorOut)
                    *errorOut = QString("Insufficient stock for %1 (%2 available)")
                                    .arg(it.value("product_name").toString())
                                    .arg(chk.value(1).toDouble());
                return {};
            }
            if (!applyDelta(db, false, productId, -qty, nullptr)) {
                db.rollback();
                if (errorOut) *errorOut = "Stock update failed";
                return {};
            }
        }
    }

    if (!db.commit()) {
        db.rollback();
        if (errorOut) *errorOut = "Commit failed";
        return {};
    }
    qDebug() << QString("sale ticket=%1 id=%2").arg(number).arg(ticketId);
    return QJsonObject{{"ok", true}, {"id", ticketId}, {"ticket_number", number}};
}

QJsonObject SalesService::openSave(const QJsonObject& req, QString* errorOut)
{
    QJsonObject t = req.value("ticket").toObject();
    QJsonArray items = req.value("items").toArray();
    if (t.isEmpty()) {
        if (errorOut) *errorOut = "Empty ticket";
        return {};
    }
    QSqlDatabase db = LocalDb::db();
    if (!db.transaction()) {
        if (errorOut) *errorOut = "The database is busy, please try again";
        return {};
    }
    QString number = claimNumber(db, t.value("ticket_number").toString());
    if (number.isEmpty()) {
        db.rollback();
        if (errorOut) *errorOut = "Ticket number collision";
        return {};
    }
    static const QStringList cols = {"ticket_number", "date", "time", "seller_username",
        "seller_display_name", "subtotal", "discount_amount", "tax_amount", "total",
        "payment_method", "status", "customer_name", "customer_phone", "deposit_amount",
        "description", "due_date"};
    QStringList holders;
    for (int i = 0; i < cols.size(); ++i) holders << "?";
    QSqlQuery ins(db);
    ins.prepare(QString("INSERT INTO tickets (%1) VALUES (%2)").arg(cols.join(","), holders.join(",")));
    for (const QString& c : cols)
        ins.addBindValue(c == "ticket_number" ? QVariant(number) : jv(t, c));
    if (!ins.exec()) {
        db.rollback();
        if (errorOut) *errorOut = ins.lastError().text();
        return {};
    }
    int ticketId = ins.lastInsertId().toInt();
    if (!insertItems(db, ticketId, items, errorOut)) {
        db.rollback();
        return {};
    }
    if (!db.commit()) {
        db.rollback();
        if (errorOut) *errorOut = "Commit failed";
        return {};
    }
    return QJsonObject{{"ok", true}, {"id", ticketId}, {"ticket_number", number}};
}

QJsonObject SalesService::openUpdate(int ticketId, const QJsonObject& req, QString* errorOut)
{
    if (ticketId <= 0) {
        if (errorOut) *errorOut = "Bad ticket id";
        return {};
    }
    QJsonObject t = req.value("ticket").toObject();
    QJsonArray items = req.value("items").toArray();
    QSqlDatabase db = LocalDb::db();
    if (!db.transaction()) {
        if (errorOut) *errorOut = "The database is busy, please try again";
        return {};
    }
    static const QStringList cols = {"subtotal", "discount_amount", "total", "customer_name",
                                     "deposit_amount", "description"};
    QStringList sets;
    for (const QString& c : cols) sets << (c + " = ?");
    QSqlQuery upd(db);
    upd.prepare(QString("UPDATE tickets SET %1 WHERE id = ?").arg(sets.join(",")));
    for (const QString& c : cols) upd.addBindValue(jv(t, c));
    upd.addBindValue(ticketId);
    if (!upd.exec() || upd.numRowsAffected() <= 0) {
        db.rollback();
        if (errorOut) *errorOut = "Ticket not found";
        return {};
    }
    QSqlQuery del(db);
    del.prepare("DELETE FROM ticket_items WHERE ticket_id = ?");
    del.addBindValue(ticketId);
    if (!del.exec()) {
        db.rollback();
        if (errorOut) *errorOut = del.lastError().text();
        return {};
    }
    if (!insertItems(db, ticketId, items, errorOut)) {
        db.rollback();
        return {};
    }
    if (!db.commit()) {
        db.rollback();
        if (errorOut) *errorOut = "Commit failed";
        return {};
    }
    return QJsonObject{{"ok", true}, {"id", ticketId}};
}

QJsonObject SalesService::openComplete(int ticketId, QString* errorOut)
{
    if (ticketId <= 0) {
        if (errorOut) *errorOut = "Bad ticket id";
        return {};
    }
    QSqlDatabase db = LocalDb::db();
    if (!db.transaction()) {
        if (errorOut) *errorOut = "The database is busy, please try again";
        return {};
    }
    // Re-read items under the transaction and verify stock first.
    QSqlQuery items(db);
    items.prepare("SELECT product_id, variant_id, quantity, product_name FROM ticket_items WHERE ticket_id = ?");
    items.addBindValue(ticketId);
    if (!items.exec()) {
        db.rollback();
        if (errorOut) *errorOut = items.lastError().text();
        return {};
    }
    struct Need { int productId; int variantId; double qty; QString name; };
    QList<Need> needs;
    while (items.next())
        needs << Need{items.value(0).toInt(), items.value(1).toInt(),
                      items.value(2).toDouble(), items.value(3).toString()};
    for (const Need& n : needs) {
        if (n.variantId > 0) {
            QSqlQuery chk(db);
            chk.prepare("SELECT pv.stock, p.is_service FROM product_variants pv "
                        "JOIN products p ON pv.parent_product_id = p.id WHERE pv.id = ?");
            chk.addBindValue(n.variantId);
            if (!chk.exec() || !chk.next()) {
                db.rollback();
                if (errorOut) *errorOut = "Variant not found";
                return {};
            }
            if (!chk.value(1).toBool() && chk.value(0).toDouble() < n.qty) {
                db.rollback();
                if (errorOut) *errorOut = "Insufficient stock for " + n.name;
                return {};
            }
        } else if (n.productId > 0) {
            QSqlQuery chk(db);
            chk.prepare("SELECT is_service, stock_quantity FROM products WHERE id = ?");
            chk.addBindValue(n.productId);
            if (!chk.exec() || !chk.next()) {
                db.rollback();
                if (errorOut) *errorOut = "Product not found";
                return {};
            }
            if (!chk.value(0).toBool() && chk.value(1).toDouble() < n.qty) {
                db.rollback();
                if (errorOut) *errorOut = "Insufficient stock for " + n.name;
                return {};
            }
        }
    }
    // The shop's own day, not UTC: a ticket paid just after midnight belongs to that day.
    QSqlQuery upd(db);
    upd.prepare("UPDATE tickets SET status = 'COMPLETED', date = date('now', 'localtime') WHERE id = ? AND status = 'OPEN'");
    upd.addBindValue(ticketId);
    if (!upd.exec() || upd.numRowsAffected() <= 0) {
        db.rollback();
        if (errorOut) *errorOut = "Ticket is no longer open";
        return {};
    }
    for (const Need& n : needs) {
        if (n.variantId > 0) {
            if (!applyDelta(db, true, n.variantId, -n.qty, nullptr)) {
                db.rollback();
                if (errorOut) *errorOut = "Stock update failed";
                return {};
            }
        } else if (n.productId > 0) {
            QSqlQuery chk(db);
            chk.prepare("SELECT is_service FROM products WHERE id = ?");
            chk.addBindValue(n.productId);
            if (chk.exec() && chk.next() && chk.value(0).toBool())
                continue;
            if (!applyDelta(db, false, n.productId, -n.qty, nullptr)) {
                db.rollback();
                if (errorOut) *errorOut = "Stock update failed";
                return {};
            }
        }
    }
    if (!db.commit()) {
        db.rollback();
        if (errorOut) *errorOut = "Commit failed";
        return {};
    }
    qDebug() << QString("open ticket completed id=%1").arg(ticketId);
    return QJsonObject{{"ok", true}, {"id", ticketId}};
}

QJsonObject SalesService::edit(int ticketId, const QJsonObject& totals,
                               const QJsonArray& items, QString* errorOut)
{
    if (ticketId <= 0) {
        if (errorOut) *errorOut = "Bad ticket id";
        return {};
    }
    QSqlDatabase db = LocalDb::db();
    if (!db.transaction()) {
        if (errorOut) *errorOut = "The database is busy, please try again";
        return {};
    }
    // Snapshot old quantities for the stock diff.
    QMap<int, double> oldP, oldV;
    {
        QSqlQuery q(db);
        q.prepare("SELECT product_id, variant_id, quantity FROM ticket_items WHERE ticket_id = ?");
        q.addBindValue(ticketId);
        if (!q.exec()) {
            db.rollback();
            if (errorOut) *errorOut = q.lastError().text();
            return {};
        }
        while (q.next()) {
            int pid = q.value(0).toInt(), vid = q.value(1).toInt();
            double qty = q.value(2).toDouble();
            if (vid > 0) oldV[vid] += qty;
            else if (pid > 0) oldP[pid] += qty;
        }
    }
    static const QStringList cols = {"subtotal", "discount_amount", "total"};
    QSqlQuery upd(db);
    upd.prepare("UPDATE tickets SET subtotal = ?, discount_amount = ?, total = ? WHERE id = ?");
    upd.addBindValue(jv(totals, "subtotal"));
    upd.addBindValue(jv(totals, "discount_amount"));
    upd.addBindValue(jv(totals, "total"));
    upd.addBindValue(ticketId);
    if (!upd.exec() || upd.numRowsAffected() <= 0) {
        db.rollback();
        if (errorOut) *errorOut = "Ticket not found";
        return {};
    }
    QSqlQuery del(db);
    del.prepare("DELETE FROM ticket_items WHERE ticket_id = ?");
    del.addBindValue(ticketId);
    if (!del.exec()) {
        db.rollback();
        if (errorOut) *errorOut = del.lastError().text();
        return {};
    }
    if (!insertItems(db, ticketId, items, errorOut)) {
        db.rollback();
        return {};
    }
    // New quantities keyed with service flags resolved per row.
    QMap<int, double> newP, newV;
    for (const QJsonValue& iv : items) {
        QJsonObject it = iv.toObject();
        if (it.value("is_service").toBool(false))
            continue;
        int vid = it.value("variant_id").toInt();
        int pid = it.value("product_id").toInt();
        double qty = it.value("quantity").toDouble();
        if (vid > 0) newV[vid] += qty;
        else if (pid > 0) newP[pid] += qty;
    }
    auto applyDiff = [&](bool isVariant, const QMap<int, double>& o,
                         const QMap<int, double>& n) -> bool {
        QSet<int> ids;
        for (auto k : o.keys()) ids.insert(k);
        for (auto k : n.keys()) ids.insert(k);
        for (int id : ids) {
            double diff = n.value(id, 0.0) - o.value(id, 0.0);
            if (diff == 0) continue;
            if (!isVariant) {
                QSqlQuery chk(db);
                chk.prepare("SELECT is_service, stock_quantity, name FROM products WHERE id = ?");
                chk.addBindValue(id);
                if (!chk.exec() || !chk.next() || chk.value(0).toBool())
                    continue;
                if (diff > 0 && chk.value(1).toDouble() < diff) {
                    if (errorOut)
                        *errorOut = QString("Insufficient stock for %1 (%2 available)")
                                        .arg(chk.value(2).toString())
                                        .arg(chk.value(1).toDouble());
                    return false;
                }
            } else if (diff > 0) {
                // Stock would decrease: verify the variant can absorb it.
                QSqlQuery chk(db);
                chk.prepare("SELECT pv.stock, p.is_service, pv.variant_name "
                            "FROM product_variants pv "
                            "JOIN products p ON pv.parent_product_id = p.id WHERE pv.id = ?");
                chk.addBindValue(id);
                if (!chk.exec() || !chk.next()) {
                    if (errorOut) *errorOut = "Variant not found";
                    return false;
                }
                if (!chk.value(1).toBool() && chk.value(0).toDouble() < diff) {
                    if (errorOut)
                        *errorOut = QString("Insufficient stock for %1 (%2 available)")
                                        .arg(chk.value(2).toString())
                                        .arg(chk.value(0).toDouble());
                    return false;
                }
            }
            double stock = 0;
            if (!applyDelta(db, isVariant, id, -diff, &stock)) {
                if (errorOut) *errorOut = "Stock update failed";
                return false;
            }
        }
        return true;
    };
    if (!applyDiff(false, oldP, newP) || !applyDiff(true, oldV, newV)) {
        db.rollback();
        return {};
    }
    if (!db.commit()) {
        db.rollback();
        if (errorOut) *errorOut = "Commit failed";
        return {};
    }
    return QJsonObject{{"ok", true}, {"id", ticketId}};
}
