#ifndef LINEICONS_H
#define LINEICONS_H

#include <QColor>
#include <QIcon>
#include <QPixmap>
#include <QString>

// Simple line icons drawn in code, so they take any theme color and stay sharp
// at any size.
namespace LineIcons {

struct Colors {
    QColor text;
    QColor muted;
    QColor accent;
};

// The icon colors of the saved theme.
Colors colors();

QPixmap pixmap(const QString& name, const QColor& color, int size = 18);
QIcon icon(const QString& name, const QColor& color, int size = 18);

// One color normally and another when a checkable button is checked.
QIcon navIcon(const QString& name, const QColor& normal, const QColor& active, int size = 18);

} // namespace LineIcons

#endif // LINEICONS_H
