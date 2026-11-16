#ifndef APPRUNTIME_H
#define APPRUNTIME_H

#include <QString>
#include <functional>

class ChangeNotifier;
class QWidget;
class LocalApi;
class LocalImages;
class UserSession;

/**
 * The startup and shutdown sequence shared by every POSBox app, so each app's
 * main() only states what is specific to it.
 */
namespace AppRuntime {

// Call first thing in main(), before constructing QApplication: sets the
// application identity and the en_US locale (number and date widgets would
// otherwise follow the system's regional settings, e.g. a decimal comma).
void configureProcess();

// Call right after constructing QApplication: selects the Fusion widget style
// (Windows would otherwise use its native style, which paints stylesheets
// differently). It has to come after construction, which would replace it.
// Also installs the UI animations.
void configureApplication();

// Who may open an app.
enum class AccessRule {
    Everyone,
    AdminOrManager,
    AdminOnly
};

struct AppConfig {
    QString name;          // "Register": also the name used in messages and deep links
    QString iconPath;
    QString lockFile;      // "register.lock"
    AccessRule access = AccessRule::Everyone;
    bool usesImages = true;
};

// What an app's window gets to work with.
struct AppContext {
    UserSession* session = nullptr;
    LocalApi* api = nullptr;
    LocalImages* images = nullptr;  // null when AppConfig::usesImages is false
    ChangeNotifier* notifier = nullptr;
};

// The window an app created, reduced to what the runtime needs from it.
struct AppWindow {
    QWidget* widget = nullptr;
    std::function<void(const QString&)> handleNavigation;  // set if --action deep links are accepted
};

// Runs the whole app: session and access checks, database, window,
// event loop, cleanup. `createWindow` receives the ready context and
// `onLogout`, which the window must call when it requests a logout.
int run(int& argc, char** argv, const AppConfig& config,
        const std::function<AppWindow(const AppContext&, const std::function<void()>&)>& createWindow);

} // namespace AppRuntime

#endif // APPRUNTIME_H
