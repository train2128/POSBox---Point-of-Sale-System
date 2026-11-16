#ifndef LOGOUTMANAGER_H
#define LOGOUTMANAGER_H

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
     * Show confirmation dialog, and if confirmed close the session via
     * SessionManager.
     *
     * @param parent Parent widget for the confirmation dialog
     * @param sessionId The session ID to close
     * @return true if logout was performed, false if user cancelled
     */
    static bool confirmAndLogout(QWidget* parent, int sessionId);
};

#endif // LOGOUTMANAGER_H
