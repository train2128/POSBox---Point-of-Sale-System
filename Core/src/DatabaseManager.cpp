#include "DatabaseManager.h"
#include "CommonConfig.h"
#include <QSqlError>
#include <QSqlQuery>
#include <QDebug>

DatabaseManager::DatabaseManager(QObject* parent, const QString& connectionName)
    : QObject(parent)
    , m_connectionName(connectionName)
{
    if (!openConnection()) {
        qCritical() << "Failed to open database connection:" << m_lastError;
    }
}

DatabaseManager::~DatabaseManager()
{
    if (m_db.isOpen()) {
        m_db.close();
        qDebug() << "Database connection closed:" << m_connectionName;
    }
}

bool DatabaseManager::openConnection()
{
    // Check if connection already exists
    if (QSqlDatabase::contains(m_connectionName)) {
        m_db = QSqlDatabase::database(m_connectionName);
        qDebug() << "Using existing database connection:" << m_connectionName;
        return m_db.isOpen();
    }

    // Create new connection
    m_db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);

    QString dbPath = POSConfig::getDatabasePath();
    if (dbPath.isEmpty()) {
        m_lastError = "Failed to determine database path";
        return false;
    }

    m_db.setDatabaseName(dbPath);

    if (!m_db.open()) {
        m_lastError = "Failed to open database: " + m_db.lastError().text();
        return false;
    }

    // Enable foreign keys
    QSqlQuery query(m_db);
    if (!query.exec("PRAGMA foreign_keys = ON")) {
        qWarning() << "Failed to enable foreign keys:" << query.lastError().text();
    }

    qDebug() << "Database connection opened:" << m_connectionName << "at:" << dbPath;
    return true;
}

bool DatabaseManager::isOpen() const
{
    return m_db.isOpen();
}

QString DatabaseManager::databasePath() const
{
    return m_db.databaseName();
}
