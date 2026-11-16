#include "LocalDb.h"
#include "CommonConfig.h"
#include "DatabaseCreator.h"
#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>

namespace {
constexpr const char* ConnectionName = "posbox_main";
}

namespace LocalDb {

bool open(QString* errorOut)
{
    if (QSqlDatabase::contains(ConnectionName) && QSqlDatabase::database(ConnectionName, false).isOpen())
        return true;

    DatabaseCreator creator;
    if (!creator.ensureDatabaseExists()) {
        if (errorOut) *errorOut = creator.lastError();
        return false;
    }

    QSqlDatabase database = QSqlDatabase::addDatabase("QSQLITE", ConnectionName);
    database.setDatabaseName(POSConfig::getDatabasePath());
    // Several POSBox apps share the file: wait for a moment instead of failing
    // when another app is writing.
    database.setConnectOptions("QSQLITE_BUSY_TIMEOUT=5000");
    if (!database.open()) {
        if (errorOut) *errorOut = database.lastError().text();
        return false;
    }
    QSqlQuery pragma(database);
    pragma.exec("PRAGMA foreign_keys = ON");
    qDebug() << "Database opened at" << POSConfig::getDatabasePath();
    return true;
}

QSqlDatabase db()
{
    return QSqlDatabase::database(ConnectionName, false);
}

void close()
{
    if (!QSqlDatabase::contains(ConnectionName))
        return;
    {
        QSqlDatabase database = QSqlDatabase::database(ConnectionName, false);
        database.close();
    }
    QSqlDatabase::removeDatabase(ConnectionName);
}

} // namespace LocalDb
