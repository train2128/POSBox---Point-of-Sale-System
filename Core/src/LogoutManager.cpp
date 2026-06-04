#include "LogoutManager.h"
#include "SessionManager.h"
#include "ActiveSessionManager.h"
#include "Prefixes.h"

bool LogoutManager::confirmAndLogout(QWidget* parent, int sessionId, const QString& username)
{
    if (!AppHelpers::confirmAction(parent, "Logout?")) {
        return false;
    }

    SessionManager::instance().closeSession(sessionId);
    ActiveSessionManager::instance().cleanupOnExit(username);

    return true;
}
