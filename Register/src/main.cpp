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
#include "ChangeNotifier.h"
#include "CommonConfig.h"
#include "ActiveSessionManager.h"
#include "RegisterWindow.h"
#include "TicketEditRequest.h"
#include "RegisterLock.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/Icons/images/icons/register_icon_colored.png"));

    QCoreApplication::setOrganizationName(POSConfig::ORG_NAME);
    QCoreApplication::setApplicationName(POSConfig::APP_NAME);

    qDebug() << "=== Register Application Starting ===";
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
    DatabaseManager* dbManager = new DatabaseManager(&app, "register_connection");
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

    // Step 5: Initialize change notifier for IPC (shared across all sessions)
    ChangeNotifier* notifier = new ChangeNotifier(&app);
    notifier->startPolling(2000);

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

        int sessionId = -1;

        if (useExistingSession) {
            qDebug() << "Session passed in from notification:" << username << "(" << userRole << ")";
            sessionId = SessionManager::instance().createSession(username, userRole, displayName);
        } else {
            LoginDialog loginDialog(authProvider, dbManager);
            StyleManager::applyGlobalStyles(&loginDialog, dbManager->database());

            if (loginDialog.exec() != QDialog::Accepted) {
                qDebug() << "Login cancelled by user";
                break;  // Exit app
            }

            if (!loginDialog.isAuthenticated()) {
                QMessageBox::warning(nullptr, "Authentication Failed",
                                     "Failed to authenticate. Please try again.");
                continue;  // Try again
            }

            qDebug() << "Login successful:" << loginDialog.username() << "(" << loginDialog.userRole() << ")";

            username = loginDialog.username();
            userRole = loginDialog.userRole();
            displayName = loginDialog.displayName();

            // Add session to active sessions
            ActiveSessionManager::SessionInfo newSession;
            newSession.username = username;
            newSession.role = userRole;
            newSession.displayName = displayName;
            newSession.appName = "Register";
            newSession.timestamp = QDateTime::currentDateTime().toString(Qt::ISODate);
            ActiveSessionManager::instance().addSession(newSession);

            sessionId = SessionManager::instance().createSession(
                loginDialog.username(),
                loginDialog.userRole(),
                loginDialog.displayName()
                );
        }

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

        // Step 6: Create and show Register window
        RegisterLock::write();
        RegisterWindow* registerWindow = new RegisterWindow(session, dbManager, notifier);

        // Connect logout signal to quit event loop and track if it was a logout
        QObject::connect(registerWindow, &RegisterWindow::logoutRequested, [&app, &wasLogout]() {
            wasLogout = true;
            app.quit();
        });

        // If a pending edit request exists, load it now
        if (TicketEditRequest::exists()) {
            registerWindow->checkPendingEditRequest();
        }

        registerWindow->show();

        qDebug() << "=== Register Application Ready ===";

        // Run event loop - logout or window close will quit this
        app.exec();

        // Cleanup
        RegisterLock::clear();
        SessionManager::instance().closeSession(sessionId);
        ActiveSessionManager::instance().cleanupOnExit(username);
        delete registerWindow;

        // If it was logout, continue loop to show login again. Otherwise exit.
        if (!wasLogout) {
            break;
        }
    }

    // Cleanup on exit
    RegisterLock::clear();
    delete dbManager;

    return 0;
}
