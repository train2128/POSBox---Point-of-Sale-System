#ifndef REGISTERLOCK_H
#define REGISTERLOCK_H

#include <QString>
#include <QFile>
#include <QTextStream>
#include "CommonConfig.h"

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <signal.h>
#endif

/**
 * Lock file helper — Register writes its PID on startup and clears it on exit.
 * Tickets reads it to decide whether to launch a new Register instance or just notify.
 */
namespace RegisterLock {

static const QString FILE_NAME = "register.lock";

inline QString filePath()
{
    return POSConfig::getAppDataPath() + FILE_NAME;
}

inline void write()
{
    QFile file(filePath());
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QTextStream(&file) << QCoreApplication::applicationPid();
    }
}

inline void clear()
{
    QFile::remove(filePath());
}

inline qint64 readPid()
{
    QFile file(filePath());
    if (!file.exists() || !file.open(QIODevice::ReadOnly))
        return -1;
    qint64 pid = -1;
    QTextStream(&file) >> pid;
    return pid;
}

inline bool isRunning()
{
    qint64 pid = readPid();
    if (pid <= 0) return false;

#ifdef Q_OS_WIN
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (!h) return false;
    DWORD code = 0;
    bool alive = GetExitCodeProcess(h, &code) && code == STILL_ACTIVE;
    CloseHandle(h);
    return alive;
#else
    // kill with signal 0 just checks existence without actually killing
    return kill(static_cast<pid_t>(pid), 0) == 0;
#endif
}

} // namespace RegisterLock

#endif // REGISTERLOCK_H
