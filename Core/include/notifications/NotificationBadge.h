#ifndef NOTIFICATIONBADGE_H
#define NOTIFICATIONBADGE_H

#include <QLabel>
#include <QPushButton>
#include <QStyle>

// Red numeric badge shown on the notification bell button (top-right).
namespace NotificationBadge {

inline QLabel* createBadge(QPushButton* button)
{
    QLabel* badge = new QLabel(button);
    badge->setAlignment(Qt::AlignCenter);
    badge->setStyleSheet(
        "QLabel {"
        "  background-color: #e74c3c;"
        "  color: white;"
        "  border-radius: 2px;"
        "  font-size: 9px;"
        "  font-weight: bold;"
        "  min-width: 14px;"
        "  max-width: 40px;"
        "  padding: 0 2px;"
        "}");
    badge->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    badge->hide();
    return badge;
}

inline void updateBadge(QLabel* badge, int count)
{
    if (!badge) return;

    QPushButton* btn = qobject_cast<QPushButton*>(badge->parentWidget());
    if (btn) {
        btn->setProperty("hasUnread", count > 0);
        btn->style()->unpolish(btn);
        btn->style()->polish(btn);
        btn->update();
    }

    if (count > 0) {
        badge->setText(count > 99 ? "99+" : QString::number(count));
        badge->adjustSize();
        if (btn) {
            badge->move(btn->width() - badge->width() - 2, -2);
        }
        badge->show();
        badge->raise();
    } else {
        badge->hide();
    }
}

} // namespace NotificationBadge

#endif // NOTIFICATIONBADGE_H