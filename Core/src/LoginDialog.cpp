#include "LoginDialog.h"
#include "ui_LoginDialog.h"
#include "IAuthenticationProvider.h"
#include "DatabaseManager.h"
#include "QueryRepository.h"
#include <QMessageBox>
#include <QDesktopServices>
#include <QProcess>
#include <QUrl>
#include <QDebug>
#include <QSqlQuery>
#include <QSqlError>

LoginDialog::LoginDialog(IAuthenticationProvider* authProvider, QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::LoginDialog())
    , m_authProvider(authProvider)
    , m_db(nullptr)
    , m_authenticated(false)
{
    ui->setupUi(this);

    // UI Configuration
    ui->UserEdit->setFocus();
    ui->PassEdit->setEchoMode(QLineEdit::Password);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    // Check if authentication provider is available
    if (!m_authProvider || !m_authProvider->isAvailable()) {
        QMessageBox::critical(this, "Authentication Error",
                              "Authentication system is not available.\n\nPlease check database connection.");
        ui->LoginButton->setEnabled(false);
    }

    // Connections
    connect(ui->LoginButton, &QPushButton::clicked, this, &LoginDialog::onLoginClicked);
    connect(ui->PassEdit, &QLineEdit::returnPressed, this, &LoginDialog::onLoginClicked);
    connect(ui->buyCoffeeBtn, &QPushButton::clicked, []() {
        const QUrl url("https://buymeacoffee.com/cheriff");
#ifdef Q_OS_LINUX
        if (QProcess::startDetached("xdg-open", {url.toString()}))
            return;
        if (QProcess::startDetached("kde-open", {url.toString()}))
            return;
#endif
        QDesktopServices::openUrl(url);
    });
}

LoginDialog::LoginDialog(IAuthenticationProvider* authProvider, DatabaseManager* db, QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::LoginDialog())
    , m_authProvider(authProvider)
    , m_db(db)
    , m_authenticated(false)
{
    ui->setupUi(this);

    ui->UserEdit->setFocus();
    ui->PassEdit->setEchoMode(QLineEdit::Password);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    if (!m_authProvider || !m_authProvider->isAvailable()) {
        showLoginError(
            "Authentication system is not available.\n\nPlease check database connection.");
        ui->LoginButton->setEnabled(false);
    }

    connect(ui->LoginButton, &QPushButton::clicked, this, &LoginDialog::onLoginClicked);
    connect(ui->PassEdit, &QLineEdit::returnPressed, this, &LoginDialog::onLoginClicked);
    connect(ui->buyCoffeeBtn, &QPushButton::clicked, []() {
        const QUrl url("https://buymeacoffee.com/cheriff");
#ifdef Q_OS_LINUX
        if (QProcess::startDetached("xdg-open", {url.toString()}))
            return;
        if (QProcess::startDetached("kde-open", {url.toString()}))
            return;
#endif
        QDesktopServices::openUrl(url);
    });
}

LoginDialog::~LoginDialog()
{
    delete ui;
}

void LoginDialog::onLoginClicked()
{
    QString username = ui->UserEdit->text().trimmed();
    QString password = ui->PassEdit->text().trimmed();

    if (username.isEmpty() || password.isEmpty()) {
        showLoginError("Please enter both username and password.");
        return;
    }

    if (!m_authProvider) {
        showLoginError("Authentication provider not available.");
        return;
    }

    // Authenticate using the provider
    AuthResult result = m_authProvider->authenticate(username, password);

    if (result.success) {
        m_authenticated = true;
        m_username = result.username;
        m_role = result.role;
        m_displayName = result.displayName;

        qDebug() << "Login successful:" << m_username << "(" << m_role << ")";

        // Run out-of-stock product cleanup
        cleanupOutOfStockProducts();

        emit userAuthenticated(m_username, m_role);
        accept(); // Close dialog with Accepted result
    } else {
        showLoginError(result.errorMessage);
    }
}

void LoginDialog::showLoginError(const QString& message)
{
    QString fullMessage = message;

    // Add hint about default credentials
    if (message.contains("Invalid", Qt::CaseInsensitive) ||
        message.contains("password", Qt::CaseInsensitive)) {
        fullMessage += "\n\nDefault credentials:\nUsername: admin\nPassword: 123";
    }

    QMessageBox::warning(this, "Login Failed", fullMessage);
    ui->PassEdit->clear();
    ui->UserEdit->selectAll();
    ui->UserEdit->setFocus();
}

void LoginDialog::cleanupOutOfStockProducts()
{
    if (!m_db || !m_db->isOpen()) return;

    // Migration: Set out_of_stock_since for existing products/variants already at 0 stock
    // This ensures they get deleted 7 days after this update is applied
    QSqlQuery migrate(m_db->database());
    migrate.exec("UPDATE products SET out_of_stock_since = CURRENT_TIMESTAMP "
                "WHERE stock_quantity = 0 AND is_service = 0 AND out_of_stock_since IS NULL");

    migrate.exec("UPDATE product_variants SET out_of_stock_since = CURRENT_TIMESTAMP "
                "WHERE stock = 0 AND out_of_stock_since IS NULL");

    // Show warning for products about to be deleted (4+ days out of stock)
    QSqlQuery warningQuery(m_db->database());
    warningQuery.exec("SELECT name, out_of_stock_since FROM products "
                     "WHERE out_of_stock_since IS NOT NULL "
                     "AND out_of_stock_since >= datetime('now', '-7 days') "
                     "AND out_of_stock_since < datetime('now', '-4 days')");

    QStringList aboutToDelete;
    while (warningQuery.next()) {
        aboutToDelete.append(warningQuery.value("name").toString());
    }

    if (!aboutToDelete.isEmpty()) {
        QStringList limitedList = aboutToDelete.size() > 5 ?
                    aboutToDelete.mid(0, 5) << "..." + QString("and %1 more").arg(aboutToDelete.size() - 5) :
                    aboutToDelete;
        QMessageBox::warning(this, "Products About to be Deleted",
            QString("The following products have been out of stock for 4+ days and will be deleted in 3 days:\n\n%1\n\nPlease restock or remove these products.")
                .arg(limitedList.join("\n")));
    }

    // Delete products out of stock for 7+ days
    QSqlQuery deleteQuery(m_db->database());
    deleteQuery.prepare(Queries::PRODUCT_SELECT_OUT_OF_STOCK_7_DAYS);
    if (!deleteQuery.exec()) {
        qWarning() << "cleanupOutOfStockProducts: failed to query:" << deleteQuery.lastError().text();
        return;
    }

    int deletedCount = 0;
    while (deleteQuery.next()) {
        int productId = deleteQuery.value("id").toInt();
        QString productName = deleteQuery.value("name").toString();

        // Delete the product
        QSqlQuery del(m_db->database());
        del.prepare("DELETE FROM products WHERE id = ?");
        del.addBindValue(productId);
        if (del.exec()) {
            deletedCount++;
            qDebug() << "cleanupOutOfStockProducts: deleted product:" << productName;
        }
    }

    if (deletedCount > 0) {
        qDebug() << "cleanupOutOfStockProducts: deleted" << deletedCount << "products";
    }
}
