#include "AppRuntime.h"
#include "InventoryWindow.h"
#include <QObject>

int main(int argc, char* argv[])
{
    AppRuntime::AppConfig config;
    config.name = "Inventory";
    config.iconPath = ":/Icons/images/icons/inventory_icon_colored.png";
    config.lockFile = "inventory.lock";
    config.access = AppRuntime::AccessRule::AdminOrManager;
    return AppRuntime::run(argc, argv, config,
        [](const AppRuntime::AppContext& context, const std::function<void()>& onLogout) {
            auto* window = new InventoryWindow(context.session, context.api, context.images, context.notifier);
            QObject::connect(window, &InventoryWindow::logoutRequested, window, onLogout);
            return AppRuntime::AppWindow{window, [window](const QString& action) {
                window->handleNavigation(action);
            }};
        });
}
