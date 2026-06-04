#ifndef NOTIFICATIONSETTINGS_H
#define NOTIFICATIONSETTINGS_H

#include <QString>

class NotificationSettings
{
public:
    struct OpenTicketSettings {
        bool enabled = true;
        int daysThreshold = 10;
        int frequencyDays = 1;
    };

    struct ExpenseSettings {
        bool enabled = true;
        int daysBefore = 3;
    };

    struct OverdueSettings {
        bool enabled = true;
        int daysOverdue = 1;
    };

    struct StockSettings {
        bool enabled = false;
    };

    OpenTicketSettings openTicket;
    ExpenseSettings expenseDue;
    OverdueSettings expenseOverdue;
    StockSettings outOfStock;
    StockSettings nearlyOutOfStock;

    static NotificationSettings load(const QString& path);
    void save(const QString& path) const;
};

#endif
