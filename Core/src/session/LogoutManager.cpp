#include "LogoutManager.h"
#include "SessionManager.h"
#include "Prefixes.h"

bool LogoutManager::confirmAndLogout(QWidget* parent, int sessionId)
{
    if (!AppHelpers::confirmAction(parent, "Logout?")) {
        return false;
    }

    // Session cleanup (active-session file + closing all apps) is handled by the
    // Launcher. Here we only close the in-memory session and let main.cpp
    // broadcast the logout request via shared memory.
    SessionManager::instance().closeSession(sessionId);

    return true;
}
