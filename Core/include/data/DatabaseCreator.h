#ifndef DATABASECREATOR_H
#define DATABASECREATOR_H

#include <QString>
#include <QSqlDatabase>

constexpr int CURRENT_UPDATE_VERSION = 1;
constexpr int CURRENT_DB_MINOR_VERSION = 1;

class DatabaseCreator
{
public:
    DatabaseCreator();
    ~DatabaseCreator();

    static int currentUpdateVersion() { return CURRENT_UPDATE_VERSION; }
    static int currentDbMinorVersion() { return CURRENT_DB_MINOR_VERSION; }

    // Main entry point - call this from every app
    bool ensureDatabaseExists();

    // Check if database file exists
    bool databaseFileExists() const;

    // Create fresh database with schema
    bool createDatabase();

    // Get last error message
    QString lastError() const { return m_lastError; }

private:
    // Schema version management
    bool createDbVersionTable(QSqlDatabase& db);
    bool getDatabaseVersion(QSqlDatabase& db, int& updateVersion, int& dbVersion);
    bool setDatabaseVersion(QSqlDatabase& db, int updateVersion, int dbVersion);
    bool needsMigration(QSqlDatabase& db);

    // Migration system
    bool runMigrations(QSqlDatabase& db);
    bool migrateToV1_3(QSqlDatabase& db);
    bool migrateToV1_4(QSqlDatabase& db);
    bool migrateToV1_5(QSqlDatabase& db);  // 2.1.5-c: theme, receipt design, notification state

    // JSON migration helpers
    void migrateSettingsJson(QSqlDatabase& db, const QString& filePath);
    void migratePrinterSettingsJson(QSqlDatabase& db, const QString& filePath);
    void migrateNotificationSettingsJson(QSqlDatabase& db, const QString& filePath);
    bool fileExists(const QString& filePath);

    // Schema creation methods
    bool createTables(QSqlDatabase& db);
    bool createIndexes(QSqlDatabase& db);
    bool createTriggers(QSqlDatabase& db);
    bool insertDefaultData(QSqlDatabase& db);

    // Individual table creation
    bool createAccountsTable(QSqlDatabase& db);
    bool createCategoriesTable(QSqlDatabase& db);
    bool createProductsTable(QSqlDatabase& db);
    bool createProductVariantsTable(QSqlDatabase& db);
    bool createTicketsTable(QSqlDatabase& db);
    bool createTicketItemsTable(QSqlDatabase& db);
    bool createDiscountsTable(QSqlDatabase& db);
    bool createRecurringExpensesTable(QSqlDatabase& db);
    bool createShopProfileTable(QSqlDatabase& db);
    bool createRegistersTable(QSqlDatabase& db);
    bool createOneTimeExpensesTable(QSqlDatabase& db);
    bool createRecurringExpensePaymentsTable(QSqlDatabase& db);
    bool createUserPermissionsTable(QSqlDatabase& db);
    bool createAppSettingsTable(QSqlDatabase& db);
    bool createPrinterSettingsTable(QSqlDatabase& db);
    bool createNotificationSettingsTable(QSqlDatabase& db);
    bool createNotificationsTable(QSqlDatabase& db);
    bool createNotificationReadsTable(QSqlDatabase& db);

    // Utility
    bool executeQuery(QSqlDatabase& db, const QString& query);
    bool addColumnIfNotExists(QSqlDatabase& db, const QString& table, const QString& column, const QString& definition);

    QString m_lastError;
};

#endif // DATABASECREATOR_H
