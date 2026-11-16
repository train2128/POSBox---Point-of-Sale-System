#ifndef SALESSERVICE_H
#define SALESSERVICE_H

#include <QJsonObject>
#include <QJsonArray>
#include <QString>

// Atomic sales operations (internal to Core). Each call validates and applies
// everything inside one SQLite transaction: no partial tickets and no sale
// can take stock below zero, even with several POSBox apps open.
class SalesService
{
public:
    // req: {"ticket":{...header incl ticket_number...}, "items":[...]}
    // Returns {"ok", "id", "ticket_number"} (number may carry a -N suffix
    // on cross-terminal collision).
    static QJsonObject complete(const QJsonObject& req, QString* errorOut);

    // Open-ticket flows (no stock movement until completion).
    static QJsonObject openSave(const QJsonObject& req, QString* errorOut);
    static QJsonObject openUpdate(int ticketId, const QJsonObject& req, QString* errorOut);
    static QJsonObject openComplete(int ticketId, QString* errorOut);

    // Completed-ticket edit: totals + item replace + stock diffs.
    static QJsonObject edit(int ticketId, const QJsonObject& totals,
                            const QJsonArray& items, QString* errorOut);

    // Ticket lifecycle helpers (Tickets app). Refund marks COMPLETED tickets
    // REFUNDED and restores stock exactly once.
    static QJsonObject refundTicket(int ticketId, QString* errorOut);
    // Deletes a ticket (items cascade). Restores stock only when the ticket is
    // still COMPLETED (a REFUNDED ticket was already restocked).
    static QJsonObject deleteTicket(int ticketId, QString* errorOut);


    // Product + variants saved together (Inventory dialog).
    // req: {"id":0|productId, "product":{cols}, "variants":[{id?, cols}],
    //       "delete_variant_ids":[ids]}
    static QJsonObject productSave(const QJsonObject& req, QString* errorOut);

    // Single stock movement + out_of_stock_since maintenance. Returns {"ok","stock"}.
    // absolute=false: delta is applied. absolute=true: deltaOrValue is the target
    // stock; the delta is computed inside the same transaction.
    static QJsonObject adjustStock(bool isVariant, int id, double deltaOrValue,
                                   bool absolute, QString* errorOut);
};

#endif // SALESSERVICE_H
