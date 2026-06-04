#include <QApplication>
#include <QMessageBox>
#include <QString>
#include <QDebug>
#include "DatabaseCreator.h"
#include "DatabaseManager.h"
#include "LocalDatabaseAuth.h"
#include "LoginDialog.h"
#include "SessionManager.h"
#include "UserSession.h"
#include "StyleManager.h"
#include "ChangeNotifier.h"
#include "CommonConfig.h"
#include "AccessControl.h"
#include "ActiveSessionManager.h"
#include "InventoryWindow.h"
#include "InventoryConstants.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/Icons/images/icons/inventory_icon_colored.png"));

    QCoreApplication::setOrganizationName(POSConfig::ORG_NAME);
    QCoreApplication::setApplicationName(POSConfig::APP_NAME);

    qDebug() << "=== Inventory Application Starting ===";
    qDebug() << "Version:" << POSConfig::APP_VERSION;

    // Step 1: Ensure database exists
    DatabaseCreator dbCreator;
    if (!dbCreator.ensureDatabaseExists()) {
        QMessageBox::critical(nullptr, "Database Error",
                              QString("Failed to initialize database:\n%1\n\nApplication will exit.")
                                  .arg(dbCreator.lastError()));
        return 1;
    }

    qDebug() << "Database verified at:" << POSConfig::getDatabasePath();

    // Step 2: Open database connection
    DatabaseManager* dbManager = new DatabaseManager(&app, "inventory_connection");
    if (!dbManager->isOpen()) {
        QMessageBox::critical(nullptr, "Database Error",
                              QString("Failed to open database:\n%1\n\nApplication will exit.")
                                  .arg(dbManager->lastError()));
        return 1;
    }

    qDebug() << "Database connection established";

    // Step 4: Create authentication provider
    LocalDatabaseAuth* authProvider = new LocalDatabaseAuth(dbManager);
    if (!authProvider->isAvailable()) {
        QMessageBox::critical(nullptr, "Authentication Error",
                              "Authentication system not available. Application will exit.");
        return 1;
    }

    qDebug() << "Authentication provider ready:" << authProvider->providerName();

    // Cleanup orphaned sessions from previous crash
    ActiveSessionManager::instance().cleanupOrphanedSessions();

    // Step 5: Initialize change notifier for IPC
    ChangeNotifier* notifier = new ChangeNotifier(&app);
    notifier->startPolling(2000);

    // Main loop - allows multiple users to log in
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

        // Step 6: Login if no existing session
        bool authenticated = false;

        while (!authenticated) {
            if (!useExistingSession) {
                LoginDialog* loginDialog = new LoginDialog(authProvider, dbManager);
                StyleManager::applyGlobalStyles(loginDialog, dbManager->database());

                if (loginDialog->exec() != QDialog::Accepted) {
                    qDebug() << "Login cancelled by user";
                    delete loginDialog;
                    return 0;
                }

                if (!loginDialog->isAuthenticated()) {
                    delete loginDialog;
                    continue;
                }

                // Check role — only Admin and Manager allowed
                AccessControl::Role role = AccessControl::stringToRole(loginDialog->userRole());
                if (role == AccessControl::Cashier) {
                    QMessageBox::warning(nullptr, "Access Denied",
                                         "You don't have permission to open Inventory app.");
                    delete loginDialog;
                    continue;
                }

                username = loginDialog->username();
                userRole = loginDialog->userRole();
                displayName = loginDialog->displayName();
                delete loginDialog;

                // Add session to active sessions
                ActiveSessionManager::SessionInfo newSession;
                newSession.username = username;
                newSession.role = userRole;
                newSession.displayName = displayName;
                newSession.appName = "Inventory";
                newSession.timestamp = QDateTime::currentDateTime().toString(Qt::ISODate);
                ActiveSessionManager::instance().addSession(newSession);
            } else {
                // Validate existing session role
                AccessControl::Role role = AccessControl::stringToRole(userRole);
                if (role == AccessControl::Cashier) {
                    QMessageBox::warning(nullptr, "Access Denied",
                                         "You don't have permission to open Inventory app.");
                    return 0;
                }
            }

            authenticated = true;
        }

        qDebug() << "Login successful:" << username << "(" << userRole << ")";

        // Step 7: Create session
        int sessionId = SessionManager::instance().createSession(
            username,
            userRole,
            displayName
            );

        if (sessionId < 0) {
            QMessageBox::critical(nullptr, "Session Error",
                                  "Failed to create user session. Application will exit.");
            break;
        }

        UserSession* session = SessionManager::instance().getSession(sessionId);
        if (!session) {
            QMessageBox::critical(nullptr, "Session Error",
                                  "Failed to retrieve session. Application will exit.");
            break;
        }

        qDebug() << "Session created:" << session->sessionInfo();

        qDebug() << "Change notifier initialized";

        // Step 8: Create and show Inventory window
        InventoryWindow* inventoryWindow = new InventoryWindow(session, dbManager, notifier);
        StyleManager::applyGlobalStyles(inventoryWindow, dbManager->database());

        // Connect logout signal to quit event loop and track if it was a logout
        QObject::connect(inventoryWindow, &InventoryWindow::logoutRequested, [&app, &wasLogout]() {
            wasLogout = true;
            app.quit();
        });

        inventoryWindow->show();

        qDebug() << "=== Inventory Application Ready ===";

        // Run event loop - logout or window close will quit this
        app.exec();

        // Cleanup
        SessionManager::instance().closeSession(sessionId);
        ActiveSessionManager::instance().cleanupOnExit(username);
        delete inventoryWindow;
        qDebug() << "Session closed:" << username;

        // If it was logout, continue loop to show login again. Otherwise exit.
        if (!wasLogout) {
            break;
        }
    }

    return 0;
}
