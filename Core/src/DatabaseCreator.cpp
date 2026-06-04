#include "DatabaseCreator.h"
#include "CommonConfig.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QFileInfo>
#include <QDebug>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

DatabaseCreator::DatabaseCreator()
{
}

DatabaseCreator::~DatabaseCreator()
{
}

bool DatabaseCreator::ensureDatabaseExists()
{
    QString dbPath = POSConfig::getDatabasePath();
    if (dbPath.isEmpty()) {
        m_lastError = "Failed to determine database path";
        return false;
    }

    if (databaseFileExists()) {
        qDebug() << "Database already exists at:" << dbPath;

        QString connectionName = "migration_connection";
        bool migrationSuccess = false;
        {
            QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
            db.setDatabaseName(dbPath);

            if (!db.open()) {
                m_lastError = "Failed to open database for migration: " + db.lastError().text();
                QSqlDatabase::removeDatabase(connectionName);
                return false;
            }

            QSqlQuery query(db);
            query.exec("PRAGMA foreign_keys = ON");

            db.transaction();
            migrationSuccess = runMigrations(db);
            if (migrationSuccess) {
                db.commit();
            } else {
                db.rollback();
            }

            db.close();
        }
        QSqlDatabase::removeDatabase(connectionName);

        if (!migrationSuccess) {
            return false;
        }

        return true;
    }

    qDebug() << "Database not found. Creating new database at:" << dbPath;
    return createDatabase();
}

bool DatabaseCreator::createDbVersionTable(QSqlDatabase& db)
{
    QString sql = R"(
        CREATE TABLE IF NOT EXISTS db_version (
            update_version INTEGER NOT NULL,
            db_version INTEGER NOT NULL,
            applied_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            PRIMARY KEY(update_version, db_version)
        )
    )";
    return executeQuery(db, sql);
}

bool DatabaseCreator::getDatabaseVersion(QSqlDatabase& db, int& updateVersion, int& dbVersion)
{
    QSqlQuery query(db);
    if (!query.exec("SELECT MAX(update_version), MAX(db_version) FROM db_version")) {
        return false;
    }
    if (query.next()) {
        updateVersion = query.value(0).toInt();
        dbVersion = query.value(1).toInt();
        return true;
    }
    return false;
}

bool DatabaseCreator::setDatabaseVersion(QSqlDatabase& db, int updateVersion, int dbVersion)
{
    QSqlQuery query(db);
    query.prepare("INSERT OR REPLACE INTO db_version (update_version, db_version, applied_at) VALUES (?, ?, CURRENT_TIMESTAMP)");
    query.addBindValue(updateVersion);
    query.addBindValue(dbVersion);
    return query.exec();
}

namespace {
    constexpr int TARGET_UPDATE_VERSION = 1;
    constexpr int TARGET_DB_VERSION = 3;
}

bool DatabaseCreator::needsMigration(QSqlDatabase& db)
{
    int updateVersion, dbVersion;
    if (getDatabaseVersion(db, updateVersion, dbVersion)) {
        if (updateVersion >= TARGET_UPDATE_VERSION && dbVersion >= TARGET_DB_VERSION) {
            return false;
        }
    }
    return true;
}

bool DatabaseCreator::runMigrations(QSqlDatabase& db)
{
    if (!needsMigration(db)) {
        qDebug() << "Database is up to date, no migration needed";
        return true;
    }

    qDebug() << "Running migration to v1.3 (from v" << TARGET_UPDATE_VERSION << "." << TARGET_DB_VERSION << ")...";

    if (!migrateToV1_3(db)) {
        m_lastError = "Migration to v1.3 failed";
        return false;
    }

    return true;
}

bool DatabaseCreator::migrateToV1_3(QSqlDatabase& db)
{
    qDebug() << "Running migration to v1.3...";

    if (!createDbVersionTable(db)) {
        qCritical() << "Failed to create db_version table:" << m_lastError;
        return false;
    }

    if (!setDatabaseVersion(db, TARGET_UPDATE_VERSION, TARGET_DB_VERSION)) {
        qCritical() << "Failed to set database version:" << m_lastError;
        return false;
    }

    if (!createUserPermissionsTable(db)) {
        qCritical() << "Failed to create user_permissions table:" << m_lastError;
        return false;
    }

    if (!createAppSettingsTable(db)) {
        qCritical() << "Failed to create app_settings table:" << m_lastError;
        return false;
    }

    addColumnIfNotExists(db, "app_settings", "last_backup", "TEXT");

    if (!createPrinterSettingsTable(db)) {
        qCritical() << "Failed to create printer_settings table:" << m_lastError;
        return false;
    }

    if (!createNotificationSettingsTable(db)) {
        qCritical() << "Failed to create notification_settings table:" << m_lastError;
        return false;
    }

    QString configPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);

    QString settingsPath = QDir(configPath).filePath("settings.json");
    if (QFile::exists(settingsPath)) {
        migrateSettingsJson(db, settingsPath);
    }

    QString printerPath = QDir(configPath).filePath("printer_settings.json");
    if (QFile::exists(printerPath)) {
        migratePrinterSettingsJson(db, printerPath);
    }

    QString notifPath = QDir(configPath).filePath("notification_settings.json");
    if (QFile::exists(notifPath)) {
        migrateNotificationSettingsJson(db, notifPath);
    }

    return true;
}



void DatabaseCreator::migrateSettingsJson(QSqlDatabase& db, const QString& filePath)
{
    QSqlQuery query(db);
    query.prepare(R"(
        CREATE TABLE IF NOT EXISTS app_settings (
            id INTEGER PRIMARY KEY CHECK(id = 1),
            color_scheme TEXT DEFAULT 'dark',
            font_size TEXT DEFAULT 'medium',
            background TEXT DEFAULT 'default',
            show_oos_products INTEGER DEFAULT 1,
            sync_path TEXT,
            last_backup TEXT
        )
    )");
    query.exec();

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "No settings.json to migrate, using defaults";
        query.prepare("INSERT OR IGNORE INTO app_settings (id) VALUES (1)");
        query.exec();
        return;
    }

    QJsonObject obj = QJsonDocument::fromJson(file.readAll()).object();
    file.close();

    if (obj.isEmpty()) {
        query.prepare("INSERT OR IGNORE INTO app_settings (id) VALUES (1)");
        query.exec();
        return;
    }

    query.prepare(R"(
        INSERT OR REPLACE INTO app_settings (id, color_scheme, font_size, background, show_oos_products)
        VALUES (1, :color_scheme, :font_size, :background, :show_oos)
    )");
    query.bindValue(":color_scheme", obj.value("color_scheme").toString("dark"));
    query.bindValue(":font_size", obj.value("font_size").toString("medium"));
    query.bindValue(":background", obj.value("background").toString("default"));
    query.bindValue(":show_oos", obj.value("show_oos_products").toBool(true) ? 1 : 0);
    query.exec();

    if (obj.contains("sync_path") || obj.contains("last_backup")) {
        query.prepare("UPDATE app_settings SET sync_path = :sync_path, last_backup = :last_backup WHERE id = 1");
        query.bindValue(":sync_path", obj.value("sync_path").toString());
        query.bindValue(":last_backup", obj.value("last_backup").toString());
        query.exec();
    }

    qDebug() << "Migrated settings.json to app_settings table";
    file.remove();
}

void DatabaseCreator::migratePrinterSettingsJson(QSqlDatabase& db, const QString& filePath)
{
    QSqlQuery query(db);
    query.prepare(R"(
        CREATE TABLE IF NOT EXISTS printer_settings (
            id INTEGER PRIMARY KEY CHECK(id = 1),
            enabled INTEGER DEFAULT 0,
            port TEXT,
            baud_rate INTEGER DEFAULT 9600,
            data_bits INTEGER DEFAULT 8,
            parity TEXT DEFAULT 'None',
            stop_bits TEXT DEFAULT '1',
            flow_control TEXT DEFAULT 'None',
            latency_ms INTEGER DEFAULT 50,
            paper_size TEXT DEFAULT '80mm',
            encoding TEXT DEFAULT 'UTF-8'
        )
    )");
    query.exec();

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "No printer_settings.json to migrate, using defaults";
        query.prepare("INSERT OR IGNORE INTO printer_settings (id) VALUES (1)");
        query.exec();
        return;
    }

    QJsonObject obj = QJsonDocument::fromJson(file.readAll()).object();
    file.close();

    if (obj.isEmpty()) {
        query.prepare("INSERT OR IGNORE INTO printer_settings (id) VALUES (1)");
        query.exec();
        return;
    }

    query.prepare(R"(
        INSERT OR REPLACE INTO printer_settings (id, enabled, port, baud_rate, data_bits, parity, stop_bits, flow_control, latency_ms, paper_size, encoding)
        VALUES (1, :enabled, :port, :baud, :data_bits, :parity, :stop_bits, :flow, :latency, :paper, :encoding)
    )");
    query.bindValue(":enabled", obj.value("printer_enabled").toBool(false) ? 1 : 0);
    query.bindValue(":port", obj.value("port").toString());
    query.bindValue(":baud", obj.value("baud_rate").toInt(9600));
    query.bindValue(":data_bits", obj.value("data_bits").toInt(8));
    query.bindValue(":parity", obj.value("parity").toString("None"));
    query.bindValue(":stop_bits", obj.value("stop_bits").toString("1"));
    query.bindValue(":flow", obj.value("flow_control").toString("None"));
    query.bindValue(":latency", obj.value("latency_ms").toInt(50));
    query.bindValue(":paper", obj.value("paper_size").toString("80mm"));
    query.bindValue(":encoding", obj.value("encoding").toString("UTF-8"));
    query.exec();

    qDebug() << "Migrated printer_settings.json to printer_settings table";
    file.remove();
}

void DatabaseCreator::migrateNotificationSettingsJson(QSqlDatabase& db, const QString& filePath)
{
    QSqlQuery query(db);
    query.prepare(R"(
        CREATE TABLE IF NOT EXISTS notification_settings (
            id INTEGER PRIMARY KEY CHECK(id = 1),
            open_ticket_enabled INTEGER DEFAULT 1,
            open_ticket_days INTEGER DEFAULT 7,
            open_ticket_frequency INTEGER DEFAULT 1,
            out_of_stock_enabled INTEGER DEFAULT 1,
            nearly_out_of_stock_enabled INTEGER DEFAULT 1
        )
    )");
    query.exec();

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "No notification_settings.json to migrate, using defaults";
        query.prepare("INSERT OR IGNORE INTO notification_settings (id) VALUES (1)");
        query.exec();
        return;
    }

    QJsonObject obj = QJsonDocument::fromJson(file.readAll()).object();
    file.close();

    if (obj.isEmpty()) {
        query.prepare("INSERT OR IGNORE INTO notification_settings (id) VALUES (1)");
        query.exec();
        return;
    }

    QJsonObject openTicket = obj.value("open_ticket").toObject();
    QJsonObject outOfStock = obj.value("out_of_stock").toObject();
    QJsonObject nearlyOutOfStock = obj.value("nearly_out_of_stock").toObject();

    query.prepare(R"(
        INSERT OR REPLACE INTO notification_settings (id,
            open_ticket_enabled, open_ticket_days, open_ticket_frequency,
            out_of_stock_enabled, nearly_out_of_stock_enabled)
        VALUES (1, :ot_enabled, :ot_days, :ot_freq,
                :oos_enabled, :nos_enabled)
    )");
    query.bindValue(":ot_enabled", openTicket.value("enabled").toBool(true) ? 1 : 0);
    query.bindValue(":ot_days", openTicket.value("days_threshold").toInt(7));
    query.bindValue(":ot_freq", openTicket.value("frequency_days").toInt(1));
    query.bindValue(":oos_enabled", outOfStock.value("enabled").toBool(true) ? 1 : 0);
    query.bindValue(":nos_enabled", nearlyOutOfStock.value("enabled").toBool(true) ? 1 : 0);
    query.exec();

    qDebug() << "Migrated notification_settings.json to notification_settings table";
    file.remove();
}

bool DatabaseCreator::databaseFileExists() const
{
    QString dbPath = POSConfig::getDatabasePath();
    QFileInfo dbFile(dbPath);
    return dbFile.exists() && dbFile.isFile();
}

bool DatabaseCreator::createDatabase()
{
    QString dbPath = POSConfig::getDatabasePath();

    // Create temporary connection for setup
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "setup_connection");
    db.setDatabaseName(dbPath);

    if (!db.open()) {
        m_lastError = "Failed to create database file: " + db.lastError().text();
        qCritical() << m_lastError;
        return false;
    }

    // Enable foreign keys
    if (!executeQuery(db, "PRAGMA foreign_keys = ON")) {
        db.close();
        QSqlDatabase::removeDatabase("setup_connection");
        return false;
    }

    // Create schema
    bool success = createTables(db) &&
                   createIndexes(db) &&
                   createTriggers(db) &&
                   insertDefaultData(db);

    // Run JSON migrations
    db.close();
    QSqlDatabase::removeDatabase("setup_connection");

    QSqlDatabase db2 = QSqlDatabase::addDatabase("QSQLITE", "json_migration");
    db2.setDatabaseName(POSConfig::getDatabasePath());
    if (db2.open()) {
        QString settingsPath = QDir(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)).filePath("settings.json");
        if (QFile::exists(settingsPath)) {
            migrateSettingsJson(db2, settingsPath);
        }
        QString printerPath = QDir(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)).filePath("printer_settings.json");
        if (QFile::exists(printerPath)) {
            migratePrinterSettingsJson(db2, printerPath);
        }
        QString notifPath = QDir(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)).filePath("notification_settings.json");
        if (QFile::exists(notifPath)) {
            migrateNotificationSettingsJson(db2, notifPath);
        }
        db2.close();
    }
    QSqlDatabase::removeDatabase("json_migration");

    // Reopen for final check
    db = QSqlDatabase::addDatabase("QSQLITE", "setup_connection");
    db.setDatabaseName(POSConfig::getDatabasePath());
    if (!db.open()) {
        qCritical() << "Failed to reopen database:" << db.lastError().text();
        return false;
    }

    if (success) {
        qDebug() << "Database created successfully!";
    } else {
        qCritical() << "Database creation failed:" << m_lastError;
    }

    return success;
}

bool DatabaseCreator::createTables(QSqlDatabase& db)
{
    return createAccountsTable(db) &&
           createCategoriesTable(db) &&
           createProductsTable(db) &&
           createProductVariantsTable(db) &&
           createDiscountsTable(db) &&
           createRecurringExpensesTable(db) &&
           createOneTimeExpensesTable(db) &&
           createRecurringExpensePaymentsTable(db) &&
           createShopProfileTable(db) &&
           createRegistersTable(db) &&
           createTicketsTable(db) &&
           createTicketItemsTable(db) &&
           createDbVersionTable(db) &&
           createUserPermissionsTable(db) &&
           createAppSettingsTable(db) &&
           createPrinterSettingsTable(db) &&
           createNotificationSettingsTable(db);
}

bool DatabaseCreator::createUserPermissionsTable(QSqlDatabase& db)
{
    QString sql = R"(
        CREATE TABLE IF NOT EXISTS user_permissions (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT NOT NULL,
            permission_key TEXT NOT NULL,
            is_allowed INTEGER NOT NULL DEFAULT -1,
            UNIQUE(username, permission_key),
            FOREIGN KEY(username) REFERENCES accounts(username) ON DELETE CASCADE
        )
    )";
    return executeQuery(db, sql);
}

bool DatabaseCreator::createAppSettingsTable(QSqlDatabase& db)
{
    QString sql = R"(
        CREATE TABLE IF NOT EXISTS app_settings (
            id INTEGER PRIMARY KEY CHECK(id = 1),
            color_scheme TEXT DEFAULT 'dark',
            font_size TEXT DEFAULT 'medium',
            background TEXT DEFAULT 'default',
            show_oos_products INTEGER DEFAULT 1,
            sync_path TEXT,
            last_backup TEXT
        )
    )";
    return executeQuery(db, sql);
}

bool DatabaseCreator::createPrinterSettingsTable(QSqlDatabase& db)
{
    QString sql = R"(
        CREATE TABLE IF NOT EXISTS printer_settings (
            id INTEGER PRIMARY KEY CHECK(id = 1),
            enabled INTEGER DEFAULT 0,
            port TEXT,
            baud_rate INTEGER DEFAULT 9600,
            data_bits INTEGER DEFAULT 8,
            parity TEXT DEFAULT 'None',
            stop_bits TEXT DEFAULT '1',
            flow_control TEXT DEFAULT 'None',
            latency_ms INTEGER DEFAULT 50,
            paper_size TEXT DEFAULT '80mm',
            encoding TEXT DEFAULT 'UTF-8'
        )
    )";
    return executeQuery(db, sql);
}

bool DatabaseCreator::createNotificationSettingsTable(QSqlDatabase& db)
{
    QString sql = R"(
        CREATE TABLE IF NOT EXISTS notification_settings (
            id INTEGER PRIMARY KEY CHECK(id = 1),
            open_ticket_enabled INTEGER DEFAULT 1,
            open_ticket_days INTEGER DEFAULT 7,
            open_ticket_frequency INTEGER DEFAULT 1,
            out_of_stock_enabled INTEGER DEFAULT 1,
            nearly_out_of_stock_enabled INTEGER DEFAULT 1
        )
    )";
    return executeQuery(db, sql);
}

bool DatabaseCreator::createAccountsTable(QSqlDatabase& db)
{
    QString sql = R"(
        CREATE TABLE IF NOT EXISTS accounts (
            username TEXT PRIMARY KEY,
            password_hash TEXT NOT NULL,
            role TEXT NOT NULL CHECK(role IN ('Admin', 'Manager', 'Cashier')),
            display_name TEXT,
            email TEXT,
            phone TEXT,
            is_active BOOLEAN DEFAULT 1,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            last_login TIMESTAMP
        )
    )";
    return executeQuery(db, sql);
}

bool DatabaseCreator::createCategoriesTable(QSqlDatabase& db)
{
    QString sql = R"(
        CREATE TABLE IF NOT EXISTS categories (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL UNIQUE,
            description TEXT,
            display_order INTEGER DEFAULT 0,
            color TEXT,
            icon_path TEXT,
            visible_in_retail BOOLEAN DEFAULT 1,
            visible_in_cafe BOOLEAN DEFAULT 1,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    )";
    return executeQuery(db, sql);
}

bool DatabaseCreator::createProductsTable(QSqlDatabase& db)
{
    QString sql = R"(
        CREATE TABLE IF NOT EXISTS products (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            description TEXT,
            cost REAL NOT NULL DEFAULT 0 CHECK(cost >= 0),
            price REAL NOT NULL CHECK(price >= 0),
            stock_quantity INTEGER DEFAULT 0,
            track_stock BOOLEAN DEFAULT 1,
            min_stock_alert INTEGER DEFAULT 0,
            barcode TEXT UNIQUE,
            sku TEXT,
            category_id INTEGER,
            is_service BOOLEAN DEFAULT 0,
            has_variants BOOLEAN DEFAULT 0,
            image_path TEXT,
            out_of_stock_since TIMESTAMP,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY(category_id) REFERENCES categories(id) ON DELETE SET NULL
        )
    )";
    return executeQuery(db, sql);
}

bool DatabaseCreator::createProductVariantsTable(QSqlDatabase& db)
{
    QString sql = R"(
        CREATE TABLE IF NOT EXISTS product_variants (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            parent_product_id INTEGER NOT NULL,
            variant_name TEXT NOT NULL,
            cost REAL NOT NULL CHECK(cost >= 0),
            price REAL NOT NULL CHECK(price >= 0),
            stock INTEGER DEFAULT 0,
            out_of_stock_since TIMESTAMP,
            barcode TEXT UNIQUE,
            sku TEXT,
            is_active BOOLEAN DEFAULT 1,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY(parent_product_id) REFERENCES products(id) ON DELETE CASCADE
        )
    )";
    return executeQuery(db, sql);
}

bool DatabaseCreator::createTicketsTable(QSqlDatabase& db)
{
    QString sql = R"(
        CREATE TABLE IF NOT EXISTS tickets (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            ticket_number TEXT NOT NULL UNIQUE,
            date DATE NOT NULL,
            time TIME NOT NULL,
            seller_username TEXT NOT NULL,
            seller_display_name TEXT,
            subtotal REAL NOT NULL CHECK(subtotal >= 0),
            discount_amount REAL DEFAULT 0 CHECK(discount_amount >= 0),
            tax_amount REAL DEFAULT 0 CHECK(tax_amount >= 0),
            total REAL NOT NULL CHECK(total >= 0),
            payment_method TEXT CHECK(payment_method IN ('CASH', 'CARD', 'MIXED')),
            status TEXT DEFAULT 'COMPLETED' CHECK(status IN ('COMPLETED', 'REFUNDED', 'CANCELLED', 'OPEN')),
            customer_name TEXT,
            customer_phone TEXT,
            deposit_amount REAL DEFAULT 0,
            description TEXT,
            due_date DATE,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    )";
    return executeQuery(db, sql);
}

bool DatabaseCreator::createTicketItemsTable(QSqlDatabase& db)
{
    QString sql = R"(
        CREATE TABLE IF NOT EXISTS ticket_items (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            ticket_id INTEGER NOT NULL,
            product_id INTEGER,
            variant_id INTEGER,
            product_name TEXT NOT NULL,
            variant_name TEXT,
            category_name TEXT,
            barcode TEXT,
            quantity REAL NOT NULL CHECK(quantity > 0),
            unit_price REAL NOT NULL CHECK(unit_price >= 0),
            unit_cost REAL DEFAULT 0 CHECK(unit_cost >= 0),
            total_price REAL NOT NULL CHECK(total_price >= 0),
            FOREIGN KEY(ticket_id) REFERENCES tickets(id) ON DELETE CASCADE
        )
    )";
    return executeQuery(db, sql);
}

bool DatabaseCreator::createDiscountsTable(QSqlDatabase& db)
{
    QString sql = R"(
        CREATE TABLE IF NOT EXISTS discounts (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            type TEXT NOT NULL CHECK(type IN ('amount', 'percentage')),
            value REAL NOT NULL CHECK(value >= 0),
            is_active BOOLEAN DEFAULT 1,
            start_date DATE,
            end_date DATE,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    )";
    return executeQuery(db, sql);
}

bool DatabaseCreator::createRecurringExpensesTable(QSqlDatabase& db)
{
    QString sql = R"(
        CREATE TABLE IF NOT EXISTS recurring_expenses (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            is_active BOOLEAN DEFAULT 1,
            type TEXT CHECK(type IN ('fixed', 'variable')),
            amount REAL CHECK(amount >= 0),
            recurrence_unit TEXT CHECK(recurrence_unit IN ('days', 'weeks', 'months', 'years')),
            recurrence_value INTEGER CHECK(recurrence_value > 0),
            start_date DATE NOT NULL,
            next_due_date DATE NOT NULL,
            notes TEXT,
            category TEXT,
            last_modified TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    )";
    return executeQuery(db, sql);
}

bool DatabaseCreator::createShopProfileTable(QSqlDatabase& db)
{
    QString sql = R"(
        CREATE TABLE IF NOT EXISTS shop_profile (
            id INTEGER PRIMARY KEY CHECK(id = 1),
            shop_name TEXT NOT NULL,
            tax_id TEXT,
            address TEXT,
            postal TEXT,
            city TEXT,
            country TEXT,
            phone TEXT,
            email TEXT,
            website TEXT,
            facebook TEXT,
            instagram TEXT,
            tiktok TEXT,
            whatsapp TEXT,
            business_mode TEXT DEFAULT 'retail' CHECK(business_mode IN ('retail', 'cafe', 'workshop')),
            currency TEXT DEFAULT 'USD',
            tax_rate REAL DEFAULT 0,
            logo_path TEXT,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    )";
    return executeQuery(db, sql);
}

bool DatabaseCreator::createRegistersTable(QSqlDatabase& db)
{
    QString sql = R"(
        CREATE TABLE IF NOT EXISTS registers (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL UNIQUE,
            description TEXT,
            is_active BOOLEAN DEFAULT 1,
            printer_name TEXT,
            cash_drawer_port TEXT,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    )";
    return executeQuery(db, sql);
}

bool DatabaseCreator::createOneTimeExpensesTable(QSqlDatabase& db)
{
    QString sql = R"(
        CREATE TABLE IF NOT EXISTS one_time_expenses (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            amount REAL NOT NULL CHECK(amount >= 0),
            date DATE NOT NULL,
            notes TEXT,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    )";
    return executeQuery(db, sql);
}

bool DatabaseCreator::createRecurringExpensePaymentsTable(QSqlDatabase& db)
{
    QString sql = R"(
        CREATE TABLE IF NOT EXISTS recurring_expense_payments (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            expense_id INTEGER NOT NULL,
            amount REAL NOT NULL CHECK(amount >= 0),
            paid_date DATE NOT NULL,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY(expense_id) REFERENCES recurring_expenses(id) ON DELETE CASCADE
        )
    )";
    return executeQuery(db, sql);
}

bool DatabaseCreator::createIndexes(QSqlDatabase& db)
{
    QStringList indexes = {
        "CREATE INDEX IF NOT EXISTS idx_products_barcode ON products(barcode)",
        "CREATE INDEX IF NOT EXISTS idx_products_category ON products(category_id)",
        "CREATE INDEX IF NOT EXISTS idx_products_track_stock ON products(track_stock)",
        "CREATE INDEX IF NOT EXISTS idx_variants_parent ON product_variants(parent_product_id)",
        "CREATE INDEX IF NOT EXISTS idx_variants_barcode ON product_variants(barcode)",
        "CREATE INDEX IF NOT EXISTS idx_tickets_date ON tickets(date)",
        "CREATE INDEX IF NOT EXISTS idx_tickets_seller ON tickets(seller_username)",
        "CREATE INDEX IF NOT EXISTS idx_tickets_status ON tickets(status)",
        "CREATE INDEX IF NOT EXISTS idx_ticket_items_ticket ON ticket_items(ticket_id)",
        "CREATE INDEX IF NOT EXISTS idx_expenses_active ON recurring_expenses(is_active)",
        "CREATE INDEX IF NOT EXISTS idx_expenses_due ON recurring_expenses(next_due_date)"
    };

    for (const QString& index : indexes) {
        if (!executeQuery(db, index)) {
            return false;
        }
    }
    return true;
}

bool DatabaseCreator::createTriggers(QSqlDatabase& db)
{
    QString profileTrigger = R"(
        CREATE TRIGGER IF NOT EXISTS update_shop_profile_timestamp
        AFTER UPDATE ON shop_profile
        FOR EACH ROW
        BEGIN
            UPDATE shop_profile SET updated_at = CURRENT_TIMESTAMP WHERE id = OLD.id;
        END
    )";

    QString productTrigger = R"(
        CREATE TRIGGER IF NOT EXISTS update_product_timestamp
        AFTER UPDATE ON products
        FOR EACH ROW
        BEGIN
            UPDATE products SET updated_at = CURRENT_TIMESTAMP WHERE id = OLD.id;
        END
    )";

    return executeQuery(db, profileTrigger) && executeQuery(db, productTrigger);
}

bool DatabaseCreator::insertDefaultData(QSqlDatabase& db)
{
    QSqlQuery query(db);

    // Insert default admin account (username: admin, password: 123)
    query.prepare("INSERT INTO accounts (username, password_hash, role, display_name) VALUES (?, ?, ?, ?)");
    query.addBindValue("admin");
    query.addBindValue("123"); // Plain text for now, will be hashed in DatabaseManager
    query.addBindValue("Admin");
    query.addBindValue("Administrator");

    if (!query.exec()) {
        m_lastError = "Failed to insert default admin account: " + query.lastError().text();
        return false;
    }

    // Insert default shop profile
    query.prepare("INSERT INTO shop_profile (id, shop_name, business_mode, currency) VALUES (1, ?, ?, ?)");
    query.addBindValue("My Store");
    query.addBindValue("retail");
    query.addBindValue("USD");

    if (!query.exec()) {
        m_lastError = "Failed to insert default shop profile: " + query.lastError().text();
        return false;
    }

    // Set initial database version
    query.prepare("INSERT INTO db_version (update_version, db_version) VALUES (?, ?)");
    query.addBindValue(CURRENT_UPDATE_VERSION);
    query.addBindValue(CURRENT_DB_MINOR_VERSION);
    if (!query.exec()) {
        m_lastError = "Failed to set database version: " + query.lastError().text();
        return false;
    }

    // Insert default settings rows
    query.exec("INSERT INTO app_settings (id) VALUES (1)");
    query.exec("INSERT INTO printer_settings (id) VALUES (1)");
    query.exec("INSERT INTO notification_settings (id) VALUES (1)");

    return true;
}

bool DatabaseCreator::executeQuery(QSqlDatabase& db, const QString& query)
{
    QSqlQuery q(db);
    if (!q.exec(query)) {
        m_lastError = "Query failed: " + q.lastError().text() + " | Query: " + query;
        qCritical() << m_lastError;
        return false;
    }
    return true;
}

bool DatabaseCreator::addColumnIfNotExists(QSqlDatabase& db, const QString& table, const QString& column, const QString& definition)
{
    QSqlQuery q(db);
    q.prepare("PRAGMA table_info(" + table + ")");
    if (q.exec()) {
        while (q.next()) {
            if (q.value(1).toString() == column) {
                return true;
            }
        }
    }
    return executeQuery(db, "ALTER TABLE " + table + " ADD COLUMN " + column + " " + definition);
}

bool DatabaseCreator::fileExists(const QString& filePath)
{
    QFileInfo fileInfo(filePath);
    return fileInfo.exists() && fileInfo.isFile();
}
