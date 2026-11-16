#include "AppRuntime.h"
#include "FinancialWindow.h"
#include <QObject>

int main(int argc, char* argv[])
{
    AppRuntime::AppConfig config;
    config.name = "Financial";
    config.iconPath = ":/Icons/images/icons/financial_icon_colored.png";
    config.lockFile = "financial.lock";
    config.access = AppRuntime::AccessRule::AdminOrManager;
    config.usesImages = false;
    return AppRuntime::run(argc, argv, config,
        [](const AppRuntime::AppContext& context, const std::function<void()>& onLogout) {
            auto* window = new FinancialWindow(context.session, context.api, context.notifier);
            QObject::connect(window, &FinancialWindow::logoutRequested, window, onLogout);
            return AppRuntime::AppWindow{window, [window](const QString& action) {
                window->handleNavigation(action);
            }};
        });
}
