#ifndef SESSIONRESOLVER_H
#define SESSIONRESOLVER_H

#include "ActiveSessionManager.h"
#include <QString>
#include <QStringList>

/**
 * Resolves the session an app was started with.
 *
 * Resolution order:
 *   1. Full session passed via command-line args (--user, --role, --display-name)
 *      -> the app was launched by the Launcher.
 *   2. An already-active session while the Launcher is running -> the app was
 *      double-clicked directly while logged in.
 *   3. Otherwise no session: the app must ask the user to login via the Launcher.
 */
namespace SessionResolver {

// Both "--key value" and "--key=value" argument forms are accepted.
ActiveSessionManager::SessionInfo resolveSession(const QStringList& args);

// Starts the Launcher; returns true if it was started.
bool launchLauncher();

} // namespace SessionResolver

#endif // SESSIONRESOLVER_H
