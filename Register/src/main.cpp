#include "AppRuntime.h"
#include "RegisterWindow.h"
#include <QObject>

int main(int argc, char* argv[])
{
    AppRuntime::AppConfig config;
    config.name = "Register";
    config.iconPath = ":/Icons/images/icons/register_icon_colored.png";
    config.lockFile = "register.lock";
    return AppRuntime::run(argc, argv, config,
        [](const AppRuntime::AppContext& context, const std::function<void()>& onLogout) {
            auto* window = new RegisterWindow(context.session, context.api, context.images, context.notifier);
            QObject::connect(window, &RegisterWindow::logoutRequested, window, onLogout);
            return AppRuntime::AppWindow{window, [window](const QString& action) {
                window->handleNavigation(action);
            }};
        });
}
