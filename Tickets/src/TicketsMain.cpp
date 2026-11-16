#include "AppRuntime.h"
#include "TicketWindow.h"
#include <QObject>

int main(int argc, char* argv[])
{
    AppRuntime::AppConfig config;
    config.name = "Tickets";
    config.iconPath = ":/Icons/images/icons/tickets_icon_colored.png";
    config.lockFile = "tickets.lock";
    return AppRuntime::run(argc, argv, config,
        [](const AppRuntime::AppContext& context, const std::function<void()>& onLogout) {
            auto* window = new TicketWindow(context.session, context.api, context.images, context.notifier);
            QObject::connect(window, &TicketWindow::logoutRequested, window, onLogout);
            return AppRuntime::AppWindow{window, {}};
        });
}
