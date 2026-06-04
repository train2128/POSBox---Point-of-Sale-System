#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>

class IAuthenticationProvider;
class DatabaseManager;

namespace Ui {
class LoginDialog;
}

/**
 * Login dialog using authentication provider abstraction
 * UI defined in LoginDialog.ui (no embedded styles)
 */
class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoginDialog(IAuthenticationProvider* authProvider, QWidget* parent = nullptr);
    explicit LoginDialog(IAuthenticationProvider* authProvider, DatabaseManager* db, QWidget* parent = nullptr);
    ~LoginDialog();

    // Getters (available after successful login)
    bool isAuthenticated() const { return m_authenticated; }
    QString username() const { return m_username; }
    QString userRole() const { return m_role; }
    QString displayName() const { return m_displayName; }

signals:
    void userAuthenticated(const QString& username, const QString& role);

private slots:
    void onLoginClicked();

private:
    void showLoginError(const QString& message);
    void cleanupOutOfStockProducts();

    Ui::LoginDialog* ui;
    IAuthenticationProvider* m_authProvider;
    DatabaseManager* m_db;

    // Authentication result
    bool m_authenticated;
    QString m_username;
    QString m_role;
    QString m_displayName;
};

#endif // LOGINDIALOG_H
