#ifndef TICKETEDITREQUEST_H
#define TICKETEDITREQUEST_H

#include <QString>
#include <QJsonObject>
#include <QJsonDocument>
#include <QFile>
#include "CommonConfig.h"

/**
 * Shared helper for passing a ticket edit request between Tickets and Register apps.
 * Uses a JSON file in the app data directory as the IPC mechanism.
 * The file is created by Tickets and deleted by Register once loaded.
 */
namespace TicketEditRequest {

static const QString FILE_NAME = "ticket_edit_request.json";

inline QString filePath()
{
    return POSConfig::getAppDataPath() + FILE_NAME;
}

/**
 * Write a ticket edit request to disk.
 * Called by the Tickets app when the user clicks Edit.
 */
inline bool write(int ticketId, const QString& requestedBy)
{
    QJsonObject obj;
    obj["ticket_id"]     = ticketId;
    obj["requested_by"]  = requestedBy;

    QFile file(filePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;

    file.write(QJsonDocument(obj).toJson());
    return true;
}

/**
 * Read the pending edit request. Returns -1 if none exists or file is invalid.
 */
inline int read(QString* requestedBy = nullptr)
{
    QFile file(filePath());
    if (!file.exists() || !file.open(QIODevice::ReadOnly))
        return -1;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isNull() || !doc.isObject())
        return -1;

    QJsonObject obj = doc.object();
    if (requestedBy)
        *requestedBy = obj.value("requested_by").toString();

    return obj.value("ticket_id").toInt(-1);
}

/**
 * Check if a pending edit request file exists.
 */
inline bool exists()
{
    return QFile::exists(filePath());
}

/**
 * Delete the request file once Register has consumed it.
 */
inline void clear()
{
    QFile::remove(filePath());
}

} // namespace TicketEditRequest

#endif // TICKETEDITREQUEST_H
