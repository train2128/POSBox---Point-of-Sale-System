#include "AppRuntime.h"
#include "AccessControl.h"
#include "Animations.h"
#include "AppLock.h"
#include "ChangeNotifier.h"
#include "CommonConfig.h"
#include "Prefixes.h"
#include "LocalApi.h"
#include "LocalDb.h"
#include "LocalImages.h"
#include "TranslationManager.h"
#include "SessionManager.h"
#include "SessionResolver.h"
#include "StyleManager.h"
#include "UserSession.h"
#include <QApplication>
#include <QDebug>
#include <QIcon>
#include <QJsonObject>
#include <QLocale>
#include <QStringList>
#include <QTabWidget>
#include <QTimer>

namespace {

bool isAllowed(AppRuntime::AccessRule rule, const QString& roleName)
{
    const AccessControl::Role role = AccessControl::stringToRole(roleName);
    switch (rule) {
    case AppRuntime::AccessRule::AdminOnly:
        return role == AccessControl::Admin;
    case AppRuntime::AccessRule::AdminOrManager:
        return role != AccessControl::Cashier;
    case AppRuntime::AccessRule::Everyone:
        break;
    }
    return true;
}

QString deepLinkArgument(const QStringList& args)
{
    for (int i = 1; i + 1 < args.size(); ++i) {
        if (args.at(i).compare("--action", Qt::CaseInsensitive) == 0)
            return args.at(i + 1);
    }
    return {};
}

} // namespace

namespace AppRuntime {

void configureProcess()
{
    // Named before QApplication: the POSBOX_DEBUG file log resolves its path
    // from these on the first Qt message, which can arrive while the platform
    // plugin is still starting up.
    QCoreApplication::setOrganizationName(POSConfig::ORG_NAME);
    QCoreApplication::setApplicationName(POSConfig::APP_NAME);
    QCoreApplication::setApplicationVersion(POSConfig::APP_VERSION);

    // Numbers are typed and shown with a decimal point whatever the system's
    // regional settings are (prices are stored that way). Month and day names
    // still come from the app language (TranslationManager sets them).
    QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
}

void configureApplication()
{
    QApplication::setStyle(QStringLiteral("Fusion"));
    Animations::install();
}

int run(int& argc, char** argv, const AppConfig& config,
        const std::function<AppWindow(const AppContext&, const std::function<void()>&)>& createWindow)
{
    configureProcess();

    QApplication app(argc, argv);
    configureApplication();
    app.setWindowIcon(QIcon(config.iconPath));
    qDebug() << "===" << config.name << "starting, version" << POSConfig::APP_VERSION << "===";

    // Session: from the Launcher's arguments, or the Launcher's active session.
    const ActiveSessionManager::SessionInfo sessionInfo =
        SessionResolver::resolveSession(app.arguments());
    if (sessionInfo.username.isEmpty()) {
        AppHelpers::showInfo(nullptr, QObject::tr("Please login first using the POSBox Launcher."), QObject::tr("Login Required"));
        SessionResolver::launchLauncher();
        return 0;
    }

    if (!isAllowed(config.access, sessionInfo.role)) {
        AppHelpers::showWarning(nullptr, QObject::tr("You don't have permission to open %1 app.").arg(config.name), QObject::tr("Access Denied"));
        return 0;
    }

    qDebug() << "Using session:" << sessionInfo.username << "(" << sessionInfo.role << ")";

    // The local database (created on first run, migrated when older).
    {
        QString err;
        if (!LocalDb::open(&err)) {
            AppHelpers::showError(nullptr, QObject::tr("Failed to open the database:\n%1\n\nApplication will exit.").arg(err), QObject::tr("Database Error"));
            return 1;
        }
    }
    AppHelpers::loadCurrencyConfig(LocalDb::db());
    TranslationManager::instance().init(LocalDb::db());

    // Single-instance guard: if this app is already running, ask the Launcher
    // to focus it and exit this duplicate instance.
    if (AppLock::isRunning(config.lockFile)) {
        ChangeNotifier duplicateNotifier;
        duplicateNotifier.notifyChange(ChangeType::APP_OPEN_REQUESTED, 0, config.name + "|show");
        return 0;
    }

    auto* api = new LocalApi(sessionInfo.username);
    LocalImages* images = config.usesImages ? new LocalImages(api, &app) : nullptr;

    auto* notifier = new ChangeNotifier(&app);
    notifier->startPolling(2000);

    const int sessionId = SessionManager::instance().createSession(
        sessionInfo.username, sessionInfo.role, sessionInfo.displayName);
    UserSession* session = SessionManager::instance().getSession(sessionId);
    if (!session) {
        AppHelpers::showError(nullptr, QObject::tr("Failed to create user session. Application will exit."), QObject::tr("Session Error"));
        return 1;
    }

    // Register this process so the Launcher knows it is running.
    AppLock::write(config.lockFile);

    // If the Launcher broadcasts a logout, close this app.
    QObject::connect(notifier, &ChangeNotifier::sessionLogoutRequested,
                     &app, &QCoreApplication::quit);

    // On logout: broadcast to all apps + Launcher, then quit.
    const std::function<void()> onLogout = [notifier, &app]() {
        notifier->notifyChange(ChangeType::SESSION_LOGOUT_REQUESTED, 0);
        app.quit();
    };

    const AppWindow window =
        createWindow(AppContext{session, api, images, notifier}, onLogout);
    if (window.widget->styleSheet().isEmpty())
        StyleManager::applySavedTheme(window.widget);
    for (QTabWidget* tabs : window.widget->findChildren<QTabWidget*>())
        Animations::attachTabs(tabs);
    window.widget->show();

    // A Wayland compositor maps a new window without raising it whenever
    // another window is focused, so the app would open hidden behind whatever
    // is on screen. Surface it once it is mapped.
    AppHelpers::bringToFront(window.widget);
    QTimer::singleShot(300, window.widget, [widget = window.widget]() {
        AppHelpers::bringToFront(widget);
    });

    // Deep link from a notification: navigate immediately after startup.
    if (window.handleNavigation) {
        const QString action = deepLinkArgument(app.arguments());
        if (!action.isEmpty())
            window.handleNavigation(action);
    }

    qDebug() << "===" << config.name << "ready ===";

    app.exec();

    AppLock::clear(config.lockFile);
    SessionManager::instance().closeSession(sessionId);
    delete window.widget;
    delete images;
    delete api;
    LocalDb::close();

    return 0;
}

} // namespace AppRuntime
