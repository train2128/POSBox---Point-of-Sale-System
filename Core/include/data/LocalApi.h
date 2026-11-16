#ifndef LOCALAPI_H
#define LOCALAPI_H

#include "CoreExport.h"
#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QVariant>
#include <QVariantMap>

using DataRow = QVariantMap;

// The data access every POSBox window uses (Commercial Version): generic
// table access plus the per-screen domain calls, all on the local database.
// Writes that touch several rows (sales, refunds, product saves, stock) run
// in one transaction. Rows are QVariantMaps keyed by column name.
class CORE_EXPORT LocalApi
{
public:
    // `username`: who is signed in (notifications are read per user).
    explicit LocalApi(const QString& username = QString());

    bool ok() const { return m_ok; }
    QString lastError() const { return m_lastError; }
    QString username() const { return m_username; }

    // Checks a user's password. {ok, username, role, display_name} on
    // success; otherwise ok() is false and lastError() says why.
    QJsonObject login(const QString& username, const QString& password);

    // Generic table access.
    QList<DataRow> select(const QString& table, const QVariantMap& where = {},
                          const QString& order = QString());
    // Returns new id (0 for text-key tables), -1 on error.
    int insert(const QString& table, const QVariantMap& values);
    bool update(const QString& table, const QVariantMap& values, const QVariantMap& where);
    bool remove(const QString& table, const QVariantMap& where);

    // Register reads.
    QList<DataRow> categories();
    QList<DataRow> products();   // product_list: join + computed stock
    QList<DataRow> variants(int productId);
    QList<DataRow> activeDiscounts();
    double dailyIncome(const QString& dateIso);  // yyyy-MM-dd
    DataRow profile();                           // shop_profile id=1
    DataRow ticket(int ticketId);
    QList<DataRow> ticketItems(int ticketId);
    QList<DataRow> openTickets();
    bool permission(const QString& username, const QString& permissionKey);
    QJsonObject stockCheckProduct(int productId);  // {is_service, stock_quantity}
    QJsonObject stockCheckVariant(int variantId);  // {stock, is_service}

    // Financial reports (month is "01".."12"). All return empty/0 on error.
    double incomeByMonth(const QString& year, const QString& month, int* countOut = nullptr);
    QList<DataRow> incomeByYear(const QString& year);  // {month, total, count}
    double ticketCostByMonth(const QString& year, const QString& month);
    QList<DataRow> topProductsByMonth(const QString& year, const QString& month);
    QList<DataRow> expensesByMonth(const QString& year, const QString& month);

    // Notifications (derived from live data, per-user read state).
    // {ok, notifications:[{id,type,context_id,title,message,read,date,action,priority}], settings:{...}}
    QJsonObject notificationsSync();
    bool notificationRead(const QString& id);
    bool notificationsReadAll();
    // Dismissal is sticky until the condition changes (which re-alerts).
    bool notificationDismiss(const QString& id);
    bool notificationsDismissAll();

    // Register writes.
    bool recordWithdrawal(const QString& displayName, double amount, const QString& dateIso);
    // Returns ticket id; ticketNumberOut carries the final (collision-safe) number.
    int completeSale(const QVariantMap& header, const QList<QVariantMap>& items, QString* ticketNumberOut);
    int saveOpenTicket(const QVariantMap& header, const QList<QVariantMap>& items, QString* ticketNumberOut);
    bool updateOpenTicket(int ticketId, const QVariantMap& header, const QList<QVariantMap>& items);
    bool completeOpenTicket(int ticketId);
    bool editTicket(int ticketId, const QVariantMap& totals, const QList<QVariantMap>& items);
    bool deleteTicket(int ticketId);  // header + items (cascade), no stock movement

    // Tickets app writes.
    bool refundTicket(int ticketId);                 // COMPLETED -> REFUNDED + stock
    bool deleteTicketWithStock(int ticketId);        // restore (if needed) + delete

    // Inventory writes.
    // Saves product + variants in one transaction. Variants carry "id" when
    // they already exist. Returns the product id, -1 on error.
    int saveProduct(const QVariantMap& product, const QList<QVariantMap>& variants,
                    const QList<int>& deleteVariantIds);
    bool setStock(bool isVariant, int id, double value);     // absolute

    // Images, stored as files in the app-data folder; the database keeps
    // file names only.
    enum class ImageKind { Product, Logo };
    QByteArray downloadImage(const QString& filename);
    bool uploadImage(const QString& filename, const QByteArray& data,
                     ImageKind kind = ImageKind::Product);
    // Absolute path of a stored image (product or logo), empty if missing.
    static QString imagePath(const QString& filename);
    static QString imageFolder(ImageKind kind);

private:
    // Runs one operation and records the outcome in ok()/lastError(). Returns
    // the result, or an empty object on failure.
    QJsonObject request(const QString& cmd, const QJsonObject& params,
                        const QString& failureMessage);
    QJsonObject named(const QString& name, const QVariantMap& args = {});

    QString m_username;
    bool m_ok = false;
    QString m_lastError;
};

#endif // LOCALAPI_H
