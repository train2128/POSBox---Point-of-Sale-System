#ifndef LOGOUTMANAGER_H
#define LOGOUTMANAGER_H

#include <QString>

class QWidget;

/**
 * Centralized logout handling for all POSBox applications.
 * Provides confirmation dialog and proper session cleanup.
 */
class LogoutManager
{
public:
    LogoutManager() = delete;
    ~LogoutManager() = delete;

    /**
     * Show confirmation dialog, and if confirmed:
     * - Close the session via SessionManager
     * - Clean up from ActiveSessionManager
     * 
     * @param parent Parent widget for the confirmation dialog
     * @param sessionId The session ID to close
     * @param username The username for ActiveSessionManager cleanup
     * @return true if logout was performed, false if user cancelled
     */
    static bool confirmAndLogout(QWidget* parent, int sessionId, const QString& username);
};

#endif // LOGOUTMANAGER_H
