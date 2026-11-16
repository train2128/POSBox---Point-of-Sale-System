#include "AppRuntime.h"
#include "settingswindow.h"
#include <QObject>

int main(int argc, char* argv[])
{
    AppRuntime::AppConfig config;
    config.name = "Settings";
    config.iconPath = ":/Icons/images/icons/settings_icon_colored.png";
    config.lockFile = "settings.lock";
    config.access = AppRuntime::AccessRule::AdminOnly;
    return AppRuntime::run(argc, argv, config,
        [](const AppRuntime::AppContext& context, const std::function<void()>& onLogout) {
            auto* window = new SettingsWindow(context.session, context.api, context.images);
            QObject::connect(window, &SettingsWindow::logoutRequested, window, onLogout);
            return AppRuntime::AppWindow{window, {}};
        });
}
