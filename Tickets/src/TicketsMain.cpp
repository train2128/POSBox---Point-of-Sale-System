#include <QApplication>
#include <QMessageBox>
#include <QStringList>
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
#include "TicketWindow.h"
#include "TicketsLock.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/Icons/images/icons/tickets_icon_colored.png"));

    QCoreApplication::setOrganizationName(POSConfig::ORG_NAME);
    QCoreApplication::setApplicationName(POSConfig::APP_NAME);

    qDebug() << "=== Tickets Application Starting ===";

    // Step 1: Ensure database exists
    DatabaseCreator dbCreator;
    if (!dbCreator.ensureDatabaseExists()) {
        QMessageBox::critical(nullptr, "Database Error",
                              QString("Failed to initialize database:\n%1\n\nApplication will exit.")
                                  .arg(dbCreator.lastError()));
        return 1;
    }

    // Step 2: Open database connection
    DatabaseManager* dbManager = new DatabaseManager(&app, "tickets_connection");
    if (!dbManager->isOpen()) {
        QMessageBox::critical(nullptr, "Database Error",
                              QString("Failed to open database:\n%1\n\nApplication will exit.")
                                  .arg(dbManager->lastError()));
        return 1;
    }

    // Step 4: Create authentication provider
    LocalDatabaseAuth* authProvider = new LocalDatabaseAuth(dbManager);
    if (!authProvider->isAvailable()) {
        QMessageBox::critical(nullptr, "Authentication Error",
                              "Authentication system not available. Application will exit.");
        return 1;
    }

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
        const QStringList args = QCoreApplication::arguments();
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

        UserSession* session = nullptr;

        if (useExistingSession) {
            qDebug() << "Session passed from notification:" << username << "(" << userRole << ")";
            int sessionId = SessionManager::instance().createSession(username, userRole, displayName);
            if (sessionId < 0) {
                QMessageBox::critical(nullptr, "Session Error",
                                      "Failed to create session. Application will exit.");
                break;
            }
            session = SessionManager::instance().getSession(sessionId);
        } else {
            // Show login dialog
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
            newSession.appName = "Tickets";
            newSession.timestamp = QDateTime::currentDateTime().toString(Qt::ISODate);
            ActiveSessionManager::instance().addSession(newSession);

            int sessionId = SessionManager::instance().createSession(
                loginDialog.username(),
                loginDialog.userRole(),
                loginDialog.displayName());

            if (sessionId < 0) {
                QMessageBox::critical(nullptr, "Session Error",
                                      "Failed to create session. Application will exit.");
                break;
            }
            session = SessionManager::instance().getSession(sessionId);
        }

        if (!session) {
            QMessageBox::critical(nullptr, "Session Error",
                                  "Failed to retrieve session. Application will exit.");
            break;
        }

        // Store sessionId for cleanup
        int sessionId = session->sessionId();

        // Step 6: Show Ticket window
        TicketsLock::write();
        TicketWindow* window = new TicketWindow(session, dbManager, notifier);

        // Connect logout signal to quit event loop and track if it was a logout
        QObject::connect(window, &TicketWindow::logoutRequested, [&app, &wasLogout]() {
            wasLogout = true;
            app.quit();
        });

        window->show();

        qDebug() << "=== Tickets Application Ready ===";

        // Run event loop - logout or window close will quit this
        app.exec();

        // Cleanup
        TicketsLock::clear();
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
