#include "NotificationSettings.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QFile>
#include <QDir>
#include <QDebug>

NotificationSettings NotificationSettings::load(const QString& path)
{
    NotificationSettings settings;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return settings;
    }

    QJsonObject obj = QJsonDocument::fromJson(file.readAll()).object();
    file.close();

    if (obj.contains("open_ticket")) {
        QJsonObject ot = obj.value("open_ticket").toObject();
        settings.openTicket.enabled = ot.value("enabled").toBool(true);
        settings.openTicket.daysThreshold = ot.value("days_threshold").toInt(10);
        settings.openTicket.frequencyDays = ot.value("frequency_days").toInt(1);
    }

    if (obj.contains("expense_due")) {
        QJsonObject ed = obj.value("expense_due").toObject();
        settings.expenseDue.enabled = ed.value("enabled").toBool(true);
        settings.expenseDue.daysBefore = ed.value("days_before").toInt(3);
    }

    if (obj.contains("expense_overdue")) {
        QJsonObject eo = obj.value("expense_overdue").toObject();
        settings.expenseOverdue.enabled = eo.value("enabled").toBool(true);
        settings.expenseOverdue.daysOverdue = eo.value("days_overdue").toInt(1);
    }

    if (obj.contains("out_of_stock")) {
        QJsonObject os = obj.value("out_of_stock").toObject();
        settings.outOfStock.enabled = os.value("enabled").toBool(false);
    }

    if (obj.contains("nearly_out_of_stock")) {
        QJsonObject nos = obj.value("nearly_out_of_stock").toObject();
        settings.nearlyOutOfStock.enabled = nos.value("enabled").toBool(false);
    }

    return settings;
}

void NotificationSettings::save(const QString& path) const
{
    QJsonObject obj;

    obj["open_ticket"] = QJsonObject{
        {"enabled", openTicket.enabled},
        {"days_threshold", openTicket.daysThreshold},
        {"frequency_days", openTicket.frequencyDays}
    };

    obj["expense_due"] = QJsonObject{
        {"enabled", expenseDue.enabled},
        {"days_before", expenseDue.daysBefore}
    };

    obj["expense_overdue"] = QJsonObject{
        {"enabled", expenseOverdue.enabled},
        {"days_overdue", expenseOverdue.daysOverdue}
    };

    obj["out_of_stock"] = QJsonObject{
        {"enabled", outOfStock.enabled}
    };

    obj["nearly_out_of_stock"] = QJsonObject{
        {"enabled", nearlyOutOfStock.enabled}
    };

    QDir dir = QFileInfo(path).dir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    QFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(obj).toJson());
        file.close();
    } else {
        qWarning() << "Failed to save notification settings:" << path;
    }
}
