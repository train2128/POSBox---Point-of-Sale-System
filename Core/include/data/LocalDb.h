#ifndef LOCALDB_H
#define LOCALDB_H

#include "CoreExport.h"
#include <QSqlDatabase>
#include <QString>

// The local SQLite database every POSBox app works on (Commercial Version).
namespace LocalDb {

// Creates the database on first run, migrates an older one, then opens this
// app's connection. Call once at startup; false with errorOut set on failure.
CORE_EXPORT bool open(QString* errorOut = nullptr);

// This app's open connection.
CORE_EXPORT QSqlDatabase db();

CORE_EXPORT void close();

} // namespace LocalDb

#endif // LOCALDB_H
