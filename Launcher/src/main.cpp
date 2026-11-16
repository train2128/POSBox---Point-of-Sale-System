#include "AppLock.h"
#include "AppRuntime.h"
#include "ChangeNotifier.h"
#include "LauncherWindow.h"
#include "LocalSettings.h"
#include "Prefixes.h"
#include "LocalApi.h"
#include "LocalDb.h"
#include "TranslationManager.h"
#include <QApplication>
#include <QIcon>
#include <QSystemTrayIcon>
#include <QTimer>

int main(int argc, char* argv[])
{
    AppRuntime::configureProcess();

    QApplication app(argc, argv);
    AppRuntime::configureApplication();
    const QIcon logo(":/Icons/images/icons/launcher_icon.png");
    app.setWindowIcon(logo);

    // Single-instance guard: if the Launcher is already running, just ask it
    // to show its window and exit.
    if (AppLock::isRunning("launcher.lock")) {
        ChangeNotifier notifier;
        notifier.notifyChange(ChangeType::APP_OPEN_REQUESTED, 0, "Launcher|show");
        return 0;
    }

    AppLock::write("launcher.lock");

    // The local database (created on first run, migrated when older).
    {
        QString err;
        if (!LocalDb::open(&err)) {
            AppHelpers::showError(nullptr,
                QObject::tr("Failed to open the database:\n%1\n\nApplication will exit.").arg(err),
                QObject::tr("Database Error"));
            AppLock::clear("launcher.lock");
            return 1;
        }
    }
    AppHelpers::loadCurrencyConfig(LocalDb::db());
    TranslationManager::instance().init(LocalDb::db());

    LocalApi api;

    auto* tray = new QSystemTrayIcon(logo, &app);

    LauncherWindow launcher(&api, tray);
    launcher.show();

    // Same as the apps: on Wayland the launcher would otherwise map behind
    // the active window and look like it never opened. Surface it.
    AppHelpers::bringToFront(&launcher);
    QTimer::singleShot(300, &launcher, [&launcher]() {
        AppHelpers::bringToFront(&launcher);
    });

    const int result = app.exec();

    AppLock::clear("launcher.lock");
    LocalDb::close();
    return result;
}
