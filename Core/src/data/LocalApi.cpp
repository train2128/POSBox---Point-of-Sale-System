#include "LocalApi.h"
#include "CommonConfig.h"
#include "NotificationsService.h"
#include "QueryService.h"
#include "SalesService.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QSaveFile>
#include "LocalDb.h"
#include <QSqlError>
#include <QSqlQuery>

namespace {

QJsonObject toObj(const QVariantMap& map)
{
    return QJsonObject::fromVariantMap(map);
}

QJsonArray toJsonItems(const QList<QVariantMap>& items)
{
    QJsonArray array;
    for (const QVariantMap& item : items)
        array.append(toObj(item));
    return array;
}

QList<DataRow> toRows(const QJsonObject& response)
{
    QList<DataRow> rows;
    const QJsonArray array = response.value("rows").toArray();
    rows.reserve(array.size());
    for (const QJsonValue& value : array)
        rows << value.toObject().toVariantMap();
    return rows;
}

QJsonObject firstRow(const QJsonObject& response)
{
    const QJsonArray rows = response.value("rows").toArray();
    return rows.isEmpty() ? QJsonObject() : rows.first().toObject();
}

// SQLite UNIQUE columns reject a second '' but allow many NULLs: empty
// barcode/sku strings must be stored as null.
QJsonObject withEmptyCodesAsNull(const QVariantMap& map)
{
    QJsonObject object = toObj(map);
    for (const char* key : {"barcode", "sku"}) {
        if (object.contains(key) && object.value(key).isString()
            && object.value(key).toString().isEmpty())
            object[key] = QJsonValue::Null;
    }
    return object;
}

// Keeps a stored file name to a plain name inside its folder.
bool safeFileName(const QString& name)
{
    return !name.isEmpty() && !name.contains('/') && !name.contains('\\')
        && name != "." && name != "..";
}

} // namespace

LocalApi::LocalApi(const QString& username)
    : m_username(username)
{
}

QJsonObject LocalApi::request(const QString& cmd, const QJsonObject& params,
                              const QString& failureMessage)
{
    m_ok = false;
    QString err;
    QJsonObject resp;

    if (cmd == "select")
        resp = QueryService::select(params, &err);
    else if (cmd == "insert")
        resp = QueryService::insert(params, &err);
    else if (cmd == "update")
        resp = QueryService::update(params, &err);
    else if (cmd == "delete")
        resp = QueryService::remove(params, &err);
    else if (cmd == "query")
        resp = QueryService::named(params, &err);
    else if (cmd == "permission_check")
        resp = QueryService::permission(params.value("username").toString(),
                                        params.value("permission_key").toString());
    else if (cmd == "adjust_stock")
        resp = SalesService::adjustStock(params.value("is_variant").toBool(false),
                                         params.value("id").toInt(),
                                         params.value("stock").toDouble(), true, &err);
    else if (cmd == "product_save")
        resp = SalesService::productSave(params, &err);
    else if (cmd == "sale_complete")
        resp = SalesService::complete(params, &err);
    else if (cmd == "ticket_open_save")
        resp = SalesService::openSave(params, &err);
    else if (cmd == "ticket_open_update")
        resp = SalesService::openUpdate(params.value("id").toInt(), params, &err);
    else if (cmd == "ticket_open_complete")
        resp = SalesService::openComplete(params.value("id").toInt(), &err);
    else if (cmd == "ticket_edit")
        resp = SalesService::edit(params.value("id").toInt(), params.value("totals").toObject(),
                                  params.value("items").toArray(), &err);
    else if (cmd == "ticket_refund")
        resp = SalesService::refundTicket(params.value("id").toInt(), &err);
    else if (cmd == "ticket_delete")
        resp = SalesService::deleteTicket(params.value("id").toInt(), &err);
    else if (cmd == "notifications_sync")
        resp = NotificationsService::sync(m_username, &err);
    else if (cmd == "notification_read")
        resp = NotificationsService::markRead(m_username, params.value("id").toString(), &err);
    else if (cmd == "notifications_read_all")
        resp = NotificationsService::markAllRead(m_username, &err);
    else if (cmd == "notification_dismiss")
        resp = NotificationsService::dismiss(m_username, params.value("id").toString(), &err);
    else if (cmd == "notifications_dismiss_all")
        resp = NotificationsService::dismissAll(m_username, &err);
    else
        err = "Unknown operation: " + cmd;

    if (!resp.value("ok").toBool(false)) {
        m_lastError = err.isEmpty() ? failureMessage : err;
        return {};
    }
    m_ok = true;
    return resp;
}

// ------------------------------------------------------------------ login ---

QJsonObject LocalApi::login(const QString& username, const QString& password)
{
    m_ok = false;
    const QString user = username.trimmed();
    QSqlQuery q(LocalDb::db());
    q.prepare("SELECT password_hash, role, display_name, is_active FROM accounts WHERE username = ?");
    q.addBindValue(user);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return {};
    }
    if (!q.next() || q.value(0).toString() != password) {
        m_lastError = "Invalid username or password";
        return {};
    }
    if (!q.value(3).toBool()) {
        m_lastError = "This account has been disabled";
        return {};
    }
    const QString role = q.value(1).toString();
    const QString displayName = q.value(2).toString().isEmpty() ? user : q.value(2).toString();
    q.finish();

    QSqlQuery stamp(LocalDb::db());
    stamp.prepare("UPDATE accounts SET last_login = CURRENT_TIMESTAMP WHERE username = ?");
    stamp.addBindValue(user);
    stamp.exec();

    m_ok = true;
    return QJsonObject{{"ok", true}, {"username", user}, {"role", role}, {"display_name", displayName}};
}

// ---------------------------------------------------------------- generic ---

QList<DataRow> LocalApi::select(const QString& table, const QVariantMap& where,
                                const QString& order)
{
    return toRows(request("select",
                          {{"table", table}, {"where", toObj(where)}, {"order", order}},
                          "Query failed"));
}

int LocalApi::insert(const QString& table, const QVariantMap& values)
{
    const QJsonObject resp = request("insert", {{"table", table}, {"values", toObj(values)}},
                                     "Insert failed");
    return m_ok ? resp.value("id").toInt(0) : -1;
}

bool LocalApi::update(const QString& table, const QVariantMap& values, const QVariantMap& where)
{
    request("update", {{"table", table}, {"values", toObj(values)}, {"where", toObj(where)}},
            "Update failed");
    return m_ok;
}

bool LocalApi::remove(const QString& table, const QVariantMap& where)
{
    request("delete", {{"table", table}, {"where", toObj(where)}}, "Delete failed");
    return m_ok;
}

// ----------------------------------------------------------------- images ---

QString LocalApi::imageFolder(ImageKind kind)
{
    const QString dir = POSConfig::getAppDataPath()
        + (kind == ImageKind::Logo ? POSConfig::LOGO_IMAGE_FOLDER : POSConfig::PRODUCT_IMAGE_FOLDER);
    QDir().mkpath(dir);
    return dir + "/";
}

QString LocalApi::imagePath(const QString& filename)
{
    if (!safeFileName(filename))
        return {};
    for (ImageKind kind : {ImageKind::Product, ImageKind::Logo}) {
        const QString path = imageFolder(kind) + filename;
        if (QFileInfo::exists(path))
            return path;
    }
    return {};
}

QByteArray LocalApi::downloadImage(const QString& filename)
{
    m_ok = false;
    const QString path = imagePath(filename);
    QFile f(path);
    if (path.isEmpty() || !f.open(QIODevice::ReadOnly)) {
        m_lastError = "Image not found";
        return {};
    }
    m_ok = true;
    return f.readAll();
}

bool LocalApi::uploadImage(const QString& filename, const QByteArray& data, ImageKind kind)
{
    m_ok = false;
    if (!safeFileName(filename) || data.isEmpty()) {
        m_lastError = "Nothing to save";
        return false;
    }
    QSaveFile f(imageFolder(kind) + filename);
    if (!f.open(QIODevice::WriteOnly) || f.write(data) != data.size() || !f.commit()) {
        m_lastError = "Could not save the image";
        return false;
    }
    m_ok = true;
    return true;
}

// ------------------------------------------------------------------ reads ---

QJsonObject LocalApi::named(const QString& name, const QVariantMap& args)
{
    QJsonObject params{{"name", name}};
    for (auto it = args.constBegin(); it != args.constEnd(); ++it) {
        const int type = it.value().typeId();
        if (type == QMetaType::Int || type == QMetaType::LongLong || type == QMetaType::Double)
            params[it.key()] = it.value().toDouble();
        else
            params[it.key()] = it.value().toString();
    }
    return request("query", params, "Query failed");
}

QList<DataRow> LocalApi::categories()
{
    return select("categories", {}, "display_order, name");
}

QList<DataRow> LocalApi::products()
{
    return toRows(named("product_list"));
}

QList<DataRow> LocalApi::variants(int productId)
{
    return select("product_variants",
                  {{"parent_product_id", productId}, {"is_active", 1}}, "variant_name");
}

QList<DataRow> LocalApi::activeDiscounts()
{
    return toRows(named("discounts_active"));
}

double LocalApi::dailyIncome(const QString& dateIso)
{
    const QJsonObject row = firstRow(named("income_by_date", {{"date", dateIso}}));
    return row.isEmpty() ? 0.0 : row.constBegin().value().toDouble();
}

DataRow LocalApi::profile()
{
    const QList<DataRow> rows = select("shop_profile", {{"id", 1}});
    return rows.isEmpty() ? DataRow() : rows.first();
}

DataRow LocalApi::ticket(int ticketId)
{
    const QList<DataRow> rows = select("tickets", {{"id", ticketId}});
    return rows.isEmpty() ? DataRow() : rows.first();
}

QList<DataRow> LocalApi::ticketItems(int ticketId)
{
    return select("ticket_items", {{"ticket_id", ticketId}});
}

QList<DataRow> LocalApi::openTickets()
{
    return select("tickets", {{"status", "OPEN"}}, "date DESC");
}

bool LocalApi::permission(const QString& username, const QString& permissionKey)
{
    const QJsonObject resp =
        request("permission_check", {{"username", username}, {"permission_key", permissionKey}},
                "Check failed");
    return m_ok && resp.value("allowed").toBool(false);
}

QJsonObject LocalApi::stockCheckProduct(int productId)
{
    return firstRow(named("stock_check_product", {{"id", productId}}));
}

QJsonObject LocalApi::stockCheckVariant(int variantId)
{
    return firstRow(named("stock_check_variant", {{"id", variantId}}));
}

double LocalApi::incomeByMonth(const QString& year, const QString& month, int* countOut)
{
    const QJsonObject row = firstRow(named("income_by_month", {{"year", year}, {"month", month}}));
    if (countOut) *countOut = row.value("count").toInt();
    return row.value("total").toDouble();
}

QList<DataRow> LocalApi::incomeByYear(const QString& year)
{
    return toRows(named("income_by_year", {{"year", year}}));
}

double LocalApi::ticketCostByMonth(const QString& year, const QString& month)
{
    return firstRow(named("ticket_cost_by_month", {{"year", year}, {"month", month}}))
        .value("cost").toDouble();
}

QList<DataRow> LocalApi::topProductsByMonth(const QString& year, const QString& month)
{
    return toRows(named("top_products_by_month", {{"year", year}, {"month", month}}));
}

QList<DataRow> LocalApi::expensesByMonth(const QString& year, const QString& month)
{
    return toRows(named("expenses_by_month", {{"year", year}, {"month", month}}));
}

QJsonObject LocalApi::notificationsSync()
{
    return request("notifications_sync", {}, "Sync failed");
}

bool LocalApi::notificationRead(const QString& id)
{
    request("notification_read", {{"id", id}}, "Read failed");
    return m_ok;
}

bool LocalApi::notificationsReadAll()
{
    request("notifications_read_all", {}, "Read failed");
    return m_ok;
}

bool LocalApi::notificationDismiss(const QString& id)
{
    request("notification_dismiss", {{"id", id}}, "Dismiss failed");
    return m_ok;
}

bool LocalApi::notificationsDismissAll()
{
    request("notifications_dismiss_all", {}, "Dismiss failed");
    return m_ok;
}

// ----------------------------------------------------------------- writes ---

bool LocalApi::recordWithdrawal(const QString& displayName, double amount, const QString& dateIso)
{
    return insert("one_time_expenses", {
        {"name", displayName}, {"amount", amount},
        {"date", dateIso}, {"notes", "Withdrawal from Register"}
    }) >= 0;
}

int LocalApi::completeSale(const QVariantMap& header, const QList<QVariantMap>& items,
                           QString* ticketNumberOut)
{
    const QJsonObject resp = request("sale_complete",
                                     {{"ticket", toObj(header)}, {"items", toJsonItems(items)}},
                                     "Sale failed");
    if (!m_ok)
        return -1;
    if (ticketNumberOut) *ticketNumberOut = resp.value("ticket_number").toString();
    return resp.value("id").toInt(-1);
}

int LocalApi::saveOpenTicket(const QVariantMap& header, const QList<QVariantMap>& items,
                             QString* ticketNumberOut)
{
    const QJsonObject resp = request("ticket_open_save",
                                     {{"ticket", toObj(header)}, {"items", toJsonItems(items)}},
                                     "Save failed");
    if (!m_ok)
        return -1;
    if (ticketNumberOut) *ticketNumberOut = resp.value("ticket_number").toString();
    return resp.value("id").toInt(-1);
}

bool LocalApi::updateOpenTicket(int ticketId, const QVariantMap& header,
                                const QList<QVariantMap>& items)
{
    request("ticket_open_update",
            {{"id", ticketId}, {"ticket", toObj(header)}, {"items", toJsonItems(items)}},
            "Update failed");
    return m_ok;
}

bool LocalApi::completeOpenTicket(int ticketId)
{
    request("ticket_open_complete", {{"id", ticketId}}, "Complete failed");
    return m_ok;
}

bool LocalApi::editTicket(int ticketId, const QVariantMap& totals, const QList<QVariantMap>& items)
{
    request("ticket_edit",
            {{"id", ticketId}, {"totals", toObj(totals)}, {"items", toJsonItems(items)}},
            "Edit failed");
    return m_ok;
}

bool LocalApi::deleteTicket(int ticketId)
{
    // The foreign key cascades ticket_items; one statement keeps it atomic.
    return remove("tickets", {{"id", ticketId}});
}

bool LocalApi::refundTicket(int ticketId)
{
    request("ticket_refund", {{"id", ticketId}}, "Refund failed");
    return m_ok;
}

bool LocalApi::deleteTicketWithStock(int ticketId)
{
    request("ticket_delete", {{"id", ticketId}}, "Delete failed");
    return m_ok;
}

int LocalApi::saveProduct(const QVariantMap& product, const QList<QVariantMap>& variants,
                          const QList<int>& deleteVariantIds)
{
    QJsonArray variantArray;
    for (const QVariantMap& variant : variants)
        variantArray.append(withEmptyCodesAsNull(variant));
    QJsonArray deleteArray;
    for (int id : deleteVariantIds)
        deleteArray.append(id);

    const QJsonObject resp = request("product_save",
                                     {{"id", product.value("id").toInt()},
                                      {"product", withEmptyCodesAsNull(product)},
                                      {"variants", variantArray},
                                      {"delete_variant_ids", deleteArray}},
                                     "Save failed");
    return m_ok ? resp.value("id").toInt(-1) : -1;
}

bool LocalApi::setStock(bool isVariant, int id, double value)
{
    request("adjust_stock", {{"is_variant", isVariant}, {"id", id}, {"stock", value}},
            "Stock update failed");
    return m_ok;
}
