#ifndef QUERYSERVICE_H
#define QUERYSERVICE_H

#include <QJsonObject>
#include <QJsonArray>
#include <QString>

// Table access and named queries on the local database (internal to Core).
// Tables and writable columns are validated against a static schema map and
// every value is bound. Returns {ok, ...} objects.
class QueryService
{
public:
    // {"rows":[...]} on success.
    static QJsonObject select(const QJsonObject& req, QString* errorOut);
    // {"allowed":bool} — role defaults plus user_permissions overrides.
    static QJsonObject permission(const QString& username, const QString& permissionKey);
    // Named read queries with fixed SQL.
    // req: {"name":"product_list"|"discounts_active"|"income_by_date"|
    //               "stock_check_product"|"stock_check_variant", ...args}
    static QJsonObject named(const QJsonObject& req, QString* errorOut);
    // {"id":lastInsertId} on success.
    static QJsonObject insert(const QJsonObject& req, QString* errorOut);
    // {"affected":n} on success.
    static QJsonObject update(const QJsonObject& req, QString* errorOut);
    static QJsonObject remove(const QJsonObject& req, QString* errorOut);

    static bool isTable(const QString& table);
};

#endif // QUERYSERVICE_H
