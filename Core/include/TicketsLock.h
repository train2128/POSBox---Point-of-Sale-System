#ifndef TICKETSLOCK_H
#define TICKETSLOCK_H

#include <QString>
#include <QFile>
#include <QTextStream>
#include <QCoreApplication>
#include "CommonConfig.h"

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <signal.h>
#endif

/**
 * Lock file helper — Tickets writes its PID on startup and clears it on exit.
 * Register reads it to decide whether to launch a new Tickets instance or just notify.
 */
namespace TicketsLock {

static const QString FILE_NAME = "tickets.lock";

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
    return kill(static_cast<pid_t>(pid), 0) == 0;
#endif
}

} // namespace TicketsLock

#endif // TICKETSLOCK_H
