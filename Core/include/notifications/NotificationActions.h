#ifndef NOTIFICATIONACTIONS_H
#define NOTIFICATIONACTIONS_H

#include <QString>
#include <QStringList>

// Shared "App|flag|value" deep-link format used by the notification panel,
// the Launcher router, and each app's handleNavigation().
namespace NotificationActions {

inline QString build(const QString& app, const QString& flag, int value = -1)
{
    if (value >= 0)
        return app + "|" + flag + "|" + QString::number(value);
    return app + "|" + flag;
}

struct Action {
    QString app;
    QString flag;
    int value = -1;
    QStringList parts;

    bool isValid() const { return !app.isEmpty() && !flag.isEmpty(); }
};

inline Action parse(const QString& action)
{
    Action a;
    a.parts = action.split('|');
    if (a.parts.isEmpty())
        return a;
    a.app = a.parts.value(0).trimmed();
    a.flag = a.parts.value(1).trimmed();
    if (a.parts.size() > 2)
        a.value = a.parts.value(2).toInt();
    return a;
}

} // namespace NotificationActions

#endif // NOTIFICATIONACTIONS_H