#include <QApplication>
#include <QMessageBox>
#include <QDebug>
#include "DatabaseCreator.h"
#include "DatabaseManager.h"
#include "LocalDatabaseAuth.h"
#include "LoginDialog.h"
#include "SessionManager.h"
#include "UserSession.h"
#include "StyleManager.h"
#include "AccessControl.h"
#include "CommonConfig.h"
#include "Prefixes.h"
#include "ActiveSessionManager.h"
#include "settingswindow.h"
#include "SettingsConstants.h"
#include "TranslationManager.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/Icons/images/icons/settings_icon_colored.png"));

    QCoreApplication::setOrganizationName(POSConfig::ORG_NAME);
    QCoreApplication::setApplicationName(POSConfig::APP_NAME);

    qDebug() << "=== Settings Application Starting ===";

    // Step 1: Ensure database exists
    DatabaseCreator dbCreator;
    if (!dbCreator.ensureDatabaseExists()) {
        QMessageBox::critical(nullptr, QObject::tr("Database Error"),
                              QString(QObject::tr("Failed to initialize database:\n%1\n\nApplication will exit."))
                                  .arg(dbCreator.lastError()));
        return 1;
    }

    qDebug() << "Database verified at:" << POSConfig::getDatabasePath();

    // Step 2: Open database connection
    DatabaseManager* dbManager = new DatabaseManager(&app, "settings_connection");
    if (!dbManager->isOpen()) {
        QMessageBox::critical(nullptr, QObject::tr("Database Error"),
                              QString(QObject::tr("Failed to open database:\n%1\n\nApplication will exit."))
                                  .arg(dbManager->lastError()));
        return 1;
    }

    qDebug() << "Database connection established";

    AppHelpers::loadCurrencyConfig(dbManager->database());
    TranslationManager::instance().init(dbManager->database());

    // Step 4: Create authentication provider
    LocalDatabaseAuth* authProvider = new LocalDatabaseAuth(dbManager);
    if (!authProvider->isAvailable()) {
        QMessageBox::critical(nullptr, QObject::tr("Authentication Error"),
                              QObject::tr("Authentication system not available. Application will exit."));
        return 1;
    }

    // Cleanup orphaned sessions from previous crash
    ActiveSessionManager::instance().cleanupOrphanedSessions();

    // Login loop - allows multiple users to log in
    while (true) {
        QString username, userRole, displayName;
        bool useExistingSession = false;
        bool wasLogout = false;

        // Check for existing session via command line
        QStringList args = app.arguments();
        for (int i = 1; i < args.size(); ++i) {
            if (args[i] == "--user" && i + 1 < args.size()) {
                username = args[++i];
                ActiveSessionManager::SessionInfo existingSession = ActiveSessionManager::instance().getSession(username);
                if (!existingSession.username.isEmpty()) {
                    userRole = existingSession.role;
                    displayName = existingSession.displayName;
                    useExistingSession = true;
                    qDebug() << "Using existing session for:" << username;
                }
            }
        }

        if (useExistingSession) {
            // Validate existing session role - Admin only
            AccessControl::Role role = AccessControl::stringToRole(userRole);
            if (role != AccessControl::Admin) {
                QMessageBox::warning(nullptr, QObject::tr("Access Denied"),
                                     QObject::tr("Access denied."));
                break;
            }
        } else {
            // Show login dialog
            LoginDialog loginDialog(authProvider, dbManager);
            StyleManager::applyGlobalStyles(&loginDialog, dbManager->database());

            if (loginDialog.exec() != QDialog::Accepted) {
                qDebug() << "Login cancelled by user";
                break;  // Exit app
            }

            if (!loginDialog.isAuthenticated()) {
                QMessageBox::warning(nullptr, QObject::tr("Authentication Failed"),
                                     QObject::tr("Failed to authenticate. Please try again."));
                continue;  // Try again
            }

            qDebug() << "Login successful:" << loginDialog.username() << "(" << loginDialog.userRole() << ")";

            username = loginDialog.username();
            userRole = loginDialog.userRole();
            displayName = loginDialog.displayName();

            // Check role - Admin only
            AccessControl::Role role = AccessControl::stringToRole(userRole);
            if (role != AccessControl::Admin) {
                QMessageBox::warning(nullptr, QObject::tr("Access Denied"),
                                     QObject::tr("Access denied."));
                continue;  // Try again with different user
            }

            // Add session to active sessions
            ActiveSessionManager::SessionInfo newSession;
            newSession.username = username;
            newSession.role = userRole;
            newSession.displayName = displayName;
            newSession.appName = "Settings";
            newSession.timestamp = QDateTime::currentDateTime().toString(Qt::ISODate);
            ActiveSessionManager::instance().addSession(newSession);
        }

        // Create session
        int sessionId = SessionManager::instance().createSession(username, userRole, displayName);

        if (sessionId < 0) {
            QMessageBox::critical(nullptr, QObject::tr("Session Error"),
                                  QObject::tr("Failed to create user session. Application will exit."));
            break;
        }

        UserSession* session = SessionManager::instance().getSession(sessionId);
        if (!session) {
            QMessageBox::critical(nullptr, QObject::tr("Session Error"),
                                  QObject::tr("Failed to retrieve session. Application will exit."));
            break;
        }

        qDebug() << "Session created:" << session->sessionInfo();

        // Step 5: Create and show Settings window
        SettingsWindow* window = new SettingsWindow(session, dbManager);
        StyleManager::applyGlobalStyles(window, dbManager->database());

        // Connect logout signal to quit event loop and track if it was a logout
        QObject::connect(window, &SettingsWindow::logoutRequested, [&app, &wasLogout]() {
            wasLogout = true;
            app.quit();
        });

        window->show();

        qDebug() << "=== Settings Application Ready ===";

        // Run event loop - logout or window close will quit this
        app.exec();

        // Cleanup
        SessionManager::instance().closeSession(sessionId);
        ActiveSessionManager::instance().cleanupOnExit(username);
        delete window;

        // If it was logout, continue loop to show login again. Otherwise exit.
        if (!wasLogout) {
            break;
        }
    }

    // Cleanup on exit
    delete dbManager;

    return 0;
}
