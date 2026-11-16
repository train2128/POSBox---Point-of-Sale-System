#ifndef ANIMATIONS_H
#define ANIMATIONS_H

#include <QPixmap>
#include <QVector>
#include <functional>

class QBoxLayout;
class QObject;
class QStackedWidget;
class QTabWidget;
class QWidget;

/**
 * The UI animations shared by every POSBox app.
 *
 * Everything is drawn by temporary child widgets that ignore the mouse and
 * delete themselves when finished, so the resting UI is exactly what the
 * stylesheets paint, and nothing depends on the platform's window system.
 * Animations are visual only: clicks and data changes always happen at once.
 *
 * The "Enable animations" setting (LocalSettings) turns all of it off.
 */
namespace Animations {

// Installs the app-wide hover, press and dialog animations. Call once, after
// constructing QApplication.
void install();

bool enabled();

// Adds a band of light that sweeps across the widget when the mouse enters it.
void enableGlare(QWidget* widget);

// Adds the sliding tab highlight and the page fade to a tab widget.
void attachTabs(QTabWidget* tabs);

// Shows another page: the old one fades out while the new one rises into place.
void switchPage(QStackedWidget* stack, int index);

// Call just before changing a widget's content: its old look fades away.
void fadeChange(QWidget* widget);

// Slides a popup's content down inside it. Call at the end of its showEvent().
void slideDownReveal(QWidget* widget);

// Grows a row just added to a layout from nothing to its full height.
void rowIn(QWidget* row);

// A picture of a row about to be removed, for collapseRow().
QPixmap rowSnapshot(QWidget* row);

// Inserts `snapshot` into `layout` at `index` and collapses it away.
void collapseRow(QBoxLayout* layout, int index, const QPixmap& snapshot);

// Counts displayed numbers up or down to `to`. `apply` receives the values to
// show and is always called last with exactly `to`. The first call for an
// owner sets the values directly, so screens don't count up when they open.
void animateValues(QObject* owner, const QVector<double>& to,
                   const std::function<void(const QVector<double>&)>& apply);

} // namespace Animations

#endif // ANIMATIONS_H
