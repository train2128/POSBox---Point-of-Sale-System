#ifndef APPLOCK_H
#define APPLOCK_H

#include <QString>

/**
 * PID lock files used by every POSBox app and the Launcher.
 *
 * Each running app writes its PID to a lock file on startup and clears it on
 * exit, so the Launcher can enumerate running apps and close them all on
 * Logout/Exit.
 *
 * A lock left behind by a crash may name a PID the OS has since given to an
 * unrelated process, so a lock only counts as "running" when the process
 * behind it is really the app the lock file is named after: "register.lock"
 * must belong to an executable called Register.
 */
namespace AppLock {

QString filePath(const QString& fileName);
void write(const QString& fileName);
void clear(const QString& fileName);
qint64 readPid(const QString& fileName);
bool isRunning(const QString& fileName);

} // namespace AppLock

#endif // APPLOCK_H
