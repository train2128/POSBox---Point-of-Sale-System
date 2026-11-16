#include "SessionResolver.h"
#include "AppLock.h"
#include "Prefixes.h"
#include <QCoreApplication>
#include <QProcess>
#include <functional>

namespace {

void forEachOption(const QStringList& args,
                   const std::function<void(const QString&, const QString&)>& visit)
{
    for (int i = 1; i < args.size(); ++i) {
        const QString& arg = args[i];
        if (arg.startsWith("--") && arg.contains('='))
            visit(arg.section('=', 0, 0), arg.section('=', 1));
        else
            visit(arg, (i + 1 < args.size()) ? args[i + 1] : QString());
    }
}

} // namespace

namespace SessionResolver {

ActiveSessionManager::SessionInfo resolveSession(const QStringList& args)
{
    ActiveSessionManager::SessionInfo info;
    forEachOption(args, [&info](const QString& key, const QString& value) {
        if (key == "--user")
            info.username = value;
        else if (key == "--role")
            info.role = value;
        else if (key == "--display-name")
            info.displayName = value;
    });

    if (!info.username.isEmpty() && !info.role.isEmpty())
        return info;

    if (AppLock::isRunning("launcher.lock")) {
        const ActiveSessionManager::SessionInfo current =
            ActiveSessionManager::instance().currentSession();
        if (!current.username.isEmpty())
            return current;
    }
    return {};
}

bool launchLauncher()
{
    const QString exePath = AppHelpers::findSiblingPath("Launcher");
    if (exePath.isEmpty())
        return false;
    return QProcess::startDetached(exePath, QStringList(),
                                   QCoreApplication::applicationDirPath());
}

} // namespace SessionResolver
