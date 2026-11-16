#include "AppLock.h"
#include "CommonConfig.h"
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <signal.h>
#endif

namespace {

// Full path of the live executable behind `pid`. Sets *alive; the path stays
// empty when the OS will not tell (the caller then trusts the PID alone).
QString liveExecutablePath(qint64 pid, bool* alive)
{
#ifdef Q_OS_WIN
    *alive = false;
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
                                 static_cast<DWORD>(pid));
    if (!process)
        return {};
    DWORD exitCode = 0;
    *alive = GetExitCodeProcess(process, &exitCode) && exitCode == STILL_ACTIVE;
    QString path;
    if (*alive) {
        wchar_t buffer[1024];
        DWORD length = 1024;
        if (QueryFullProcessImageNameW(process, 0, buffer, &length))
            path = QString::fromWCharArray(buffer, static_cast<int>(length));
    }
    CloseHandle(process);
    return path;
#else
    *alive = (kill(static_cast<pid_t>(pid), 0) == 0);
    if (!*alive)
        return {};
    // Linux only; elsewhere the link does not exist and the PID alone decides.
    return QFile::symLinkTarget(QStringLiteral("/proc/%1/exe").arg(pid));
#endif
}

} // namespace

namespace AppLock {

QString filePath(const QString& fileName)
{
    return POSConfig::getAppDataPath() + fileName;
}

void write(const QString& fileName)
{
    QFile file(filePath(fileName));
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        QTextStream(&file) << QCoreApplication::applicationPid();
}

void clear(const QString& fileName)
{
    QFile::remove(filePath(fileName));
}

qint64 readPid(const QString& fileName)
{
    QFile file(filePath(fileName));
    if (!file.open(QIODevice::ReadOnly))
        return -1;
    qint64 pid = -1;
    QTextStream(&file) >> pid;
    return pid;
}

bool isRunning(const QString& fileName)
{
    const qint64 pid = readPid(fileName);
    if (pid <= 0)
        return false;

    bool alive = false;
    const QString exe = liveExecutablePath(pid, &alive);
    if (!alive)
        return false;
    if (exe.isEmpty())
        return true;

    // "register.lock" <-> ".../Register" or "...\Register.exe"
    const QString appName = QFileInfo(fileName).baseName();
    return QFileInfo(exe).fileName().startsWith(appName, Qt::CaseInsensitive);
}

} // namespace AppLock
