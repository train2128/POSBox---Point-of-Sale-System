#include "Animations.h"
#include "TranslationManager.h"
#include <QCalendarWidget>
#include <QDateEdit>
#include "ComboStyle.h"
#include "LocalSettings.h"
#include <QApplication>
#include <QBoxLayout>
#include <QComboBox>
#include <QDialog>
#include <QPainterPath>
#include <QEasingCurve>
#include <QEvent>
#include <QImage>
#include <QLabel>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPointer>
#include <QPushButton>
#include <QStackedWidget>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QVariantAnimation>
#include <memory>

namespace {

constexpr int HoverFadeMs = 120;
constexpr int PressFadeInMs = 60;
constexpr int PressStateFadeMs = 90;
constexpr int ReleaseFadeMs = 140;
constexpr int PressReleaseWatchdogMs = 2000;
constexpr qreal PressDimOpacity = 0.22;
constexpr int GlareMs = 450;
constexpr int GlareBandAlpha = 95;
constexpr int TabSlideMs = 180;
constexpr int TabHighlightFadeMs = 140;
constexpr int PageFadeMs = 160;
constexpr int DialogFadeMs = 140;
constexpr int PanelSlideMs = 170;
constexpr qreal RoundedWindowRadius = 15;
constexpr int RowMs = 150;
constexpr int ValueMs = 250;
constexpr int PageSwitchMs = 190;
constexpr int PageRiseOffset = 14;
constexpr int ContentFadeMs = 150;

const char* const GlareProperty = "posboxAnimGlare";
const char* const ValuesProperty = "posboxAnimTarget";
const char* const ShownProperty = "posboxAnimShown";
const char* const GenerationProperty = "posboxAnimGeneration";

void tween(QObject* owner, int ms, QEasingCurve::Type curve,
           std::function<void(qreal)> step, std::function<void()> done = {})
{
    step(0.0);
    auto* animation = new QVariantAnimation(owner);
    animation->setDuration(ms);
    animation->setStartValue(0.0);
    animation->setEndValue(1.0);
    animation->setEasingCurve(curve);
    QObject::connect(animation, &QVariantAnimation::valueChanged, owner,
                     [step](const QVariant& value) { step(value.toDouble()); });
    if (done)
        QObject::connect(animation, &QAbstractAnimation::finished, owner, done);
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}

enum class OverlayRole { Other, Hover, PressState, PressDim };

// A temporary layer over a widget. Ignores the mouse; removed when finished.
class Overlay : public QWidget
{
public:
    std::function<void(QPainter&)> paint;
    const OverlayRole role;

    explicit Overlay(QWidget* parent, OverlayRole overlayRole = OverlayRole::Other,
                     const QRect& geometry = QRect())
        : QWidget(parent)
        , role(overlayRole)
    {
        setObjectName("posboxAnimOverlay");
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        setFocusPolicy(Qt::NoFocus);
        setGeometry(geometry.isNull() ? parent->rect() : geometry);
        show();
        raise();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        if (paint)
            paint(painter);
    }
};

Overlay* findOverlay(QWidget* widget, OverlayRole role)
{
    for (QObject* child : widget->children()) {
        auto* overlay = dynamic_cast<Overlay*>(child);
        if (overlay && overlay->role == role)
            return overlay;
    }
    return nullptr;
}

// Like QWidget::grab(), but without filling the rectangle with the window
// color, so rounded corners and translucent backgrounds stay transparent.
QPixmap snapshot(QWidget* widget)
{
    const qreal ratio = widget->devicePixelRatioF();
    QPixmap pixmap(widget->size() * ratio);
    pixmap.setDevicePixelRatio(ratio);
    pixmap.fill(Qt::transparent);
    widget->render(&pixmap, QPoint(), QRegion(), QWidget::DrawChildren);
    return pixmap;
}

QPixmap snapshotWithoutOverlays(QWidget* widget)
{
    QList<Overlay*> hidden;
    for (QObject* child : widget->children()) {
        auto* overlay = dynamic_cast<Overlay*>(child);
        if (overlay && overlay->isVisible()) {
            overlay->hide();
            hidden << overlay;
        }
    }
    const QPixmap pixmap = snapshot(widget);
    for (Overlay* overlay : hidden)
        overlay->show();
    return pixmap;
}

// The widget's shape: opaque wherever it paints anything, so effects follow
// its rounded corners even when its background is nearly transparent.
QPixmap shapeMask(const QPixmap& pixmap)
{
    QImage image = pixmap.toImage().convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < image.height(); ++y) {
        auto* line = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x)
            line[x] = qAlpha(line[x]) ? qRgba(255, 255, 255, 255) : 0;
    }
    QPixmap mask = QPixmap::fromImage(image);
    mask.setDevicePixelRatio(pixmap.devicePixelRatio());
    return mask;
}

// Paints `brush` clipped to `mask`.
QPixmap maskedFill(const QPixmap& mask, const QBrush& brush)
{
    QPixmap buffer(mask.size());
    buffer.setDevicePixelRatio(mask.devicePixelRatio());
    buffer.fill(Qt::transparent);
    QPainter painter(&buffer);
    painter.fillRect(QRectF(QPointF(0, 0), QSizeF(mask.size()) / mask.devicePixelRatio()), brush);
    painter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
    painter.drawPixmap(0, 0, mask);
    return buffer;
}

void removeWhenDone(const QPointer<Overlay>& overlay)
{
    if (overlay)
        overlay->deleteLater();
}

// Fades a widget's previous look out over its new look.
void crossfade(QWidget* widget, const QPixmap& previous, int ms, OverlayRole role)
{
    delete findOverlay(widget, role);
    auto* overlay = new Overlay(widget, role);
    auto opacity = std::make_shared<qreal>(1.0);
    overlay->paint = [previous, opacity](QPainter& painter) {
        painter.setOpacity(*opacity);
        painter.drawPixmap(0, 0, previous);
    };
    QPointer<Overlay> guard(overlay);
    tween(overlay, ms, QEasingCurve::OutCubic,
          [guard, opacity](qreal t) { *opacity = 1.0 - t; if (guard) guard->update(); },
          [guard] { removeWhenDone(guard); });
}

void sweepGlare(QWidget* widget)
{
    const QPixmap mask = shapeMask(snapshotWithoutOverlays(widget));
    auto* overlay = new Overlay(widget);
    auto position = std::make_shared<qreal>(0.0);
    overlay->paint = [mask, position, overlay](QPainter& painter) {
        const qreal span = overlay->width() + overlay->height();
        const qreal start = -0.25 * span + *position * 1.5 * span;
        QLinearGradient band(start, 0, start + 0.30 * span, 0.30 * span);
        band.setColorAt(0.0, QColor(255, 255, 255, 0));
        band.setColorAt(0.5, QColor(255, 255, 255, GlareBandAlpha));
        band.setColorAt(1.0, QColor(255, 255, 255, 0));
        painter.drawPixmap(0, 0, maskedFill(mask, band));
    };
    QPointer<Overlay> guard(overlay);
    tween(overlay, GlareMs, QEasingCurve::InOutSine,
          [guard, position](qreal t) { *position = t; if (guard) guard->update(); },
          [guard] { removeWhenDone(guard); });
}

// The press dim keeps its current strength as a property of the overlay.
void fadeOutPressDim(Overlay* overlay)
{
    if (!overlay || overlay->property("fadingOut").toBool())
        return;
    overlay->setProperty("fadingOut", true);
    const qreal from = overlay->property("strength").toDouble();
    QPointer<Overlay> guard(overlay);
    tween(overlay, ReleaseFadeMs, QEasingCurve::OutQuad,
          [guard, from](qreal t) {
              if (!guard) return;
              guard->setProperty("strength", from * (1.0 - t));
              guard->update();
          },
          [guard] { removeWhenDone(guard); });
}

void startPressDim(QWidget* widget, const QPixmap& look, bool releasesItself)
{
    delete findOverlay(widget, OverlayRole::PressDim);
    const QPixmap dark = maskedFill(shapeMask(look), Qt::black);
    auto* overlay = new Overlay(widget, OverlayRole::PressDim);
    overlay->paint = [dark, overlay](QPainter& painter) {
        painter.setOpacity(overlay->property("strength").toDouble());
        painter.drawPixmap(0, 0, dark);
    };
    QPointer<Overlay> guard(overlay);
    tween(overlay, PressFadeInMs, QEasingCurve::OutQuad,
          [guard](qreal t) {
              if (!guard || guard->property("fadingOut").toBool()) return;
              guard->setProperty("strength", PressDimOpacity * t);
              guard->update();
          },
          [guard, releasesItself] { if (releasesItself) fadeOutPressDim(guard); });

    // A release may never arrive (the click opened a dialog, or disabled the
    // widget): never leave the widget looking pressed.
    QTimer::singleShot(PressReleaseWatchdogMs, overlay, [guard] { fadeOutPressDim(guard); });
}

bool isProductTile(const QWidget* widget)
{
    return widget->objectName() == QLatin1String("product-button");
}

QWidget* feedbackTarget(QObject* object)
{
    auto* widget = qobject_cast<QWidget*>(object);
    if (!widget || !widget->isEnabled() || widget->size().isEmpty()
        || widget->property("posboxNoFeedback").toBool())
        return nullptr;
    return (qobject_cast<QPushButton*>(widget) || isProductTile(widget)) ? widget : nullptr;
}

bool fadesIn(const QDialog* dialog)
{
    // Native dialogs (file, print) are not painted by Qt, and the Launcher's
    // login window is the app's main window rather than a dialog on top.
    return !dialog->testAttribute(Qt::WA_DontShowOnScreen)
        && dialog->objectName() != QLatin1String("launcherWindow")
        && !dialog->inherits("QFileDialog")
        && !dialog->inherits("QAbstractPrintDialog")
        && !dialog->inherits("QPrintPreviewDialog")
        && !dialog->inherits("QPageSetupDialog");
}

void fadeInDialog(QDialog* dialog)
{
    const QImage look = dialog->grab().toImage();
    if (look.isNull())
        return;
    // Windows with a painted shadow are only covered over their card.
    const int margin = dialog->property("posboxCardMargin").toInt();
    const QColor background = look.pixelColor(look.width() / 2, look.height() - 3 - margin);
    auto* overlay = new Overlay(dialog);
    auto opacity = std::make_shared<qreal>(1.0);
    overlay->paint = [background, opacity, overlay, margin](QPainter& painter) {
        painter.setOpacity(*opacity);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(background);
        const qreal radius = margin > 0 ? RoundedWindowRadius : 0;
        painter.drawRoundedRect(QRectF(overlay->rect()).adjusted(margin, margin, -margin, -margin), radius, radius);
    };
    QPointer<Overlay> guard(overlay);
    tween(overlay, DialogFadeMs, QEasingCurve::OutCubic,
          [guard, opacity](qreal t) { *opacity = 1.0 - t; if (guard) guard->update(); },
          [guard] { removeWhenDone(guard); });
}

class Controller : public QObject
{
public:
    using QObject::QObject;

    bool eventFilter(QObject* object, QEvent* event) override
    {
        switch (event->type()) {
        case QEvent::Enter:
        case QEvent::Leave:
            if (QWidget* widget = feedbackTarget(object); widget && Animations::enabled())
                onHoverChange(widget, event->type() == QEvent::Enter);
            break;
        case QEvent::MouseButtonPress:
            if (QWidget* widget = feedbackTarget(object); widget && Animations::enabled())
                onPress(widget);
            break;
        case QEvent::MouseButtonRelease:
            if (QWidget* widget = feedbackTarget(object))
                onRelease(widget);
            break;
        case QEvent::Polish:
            if (auto* combo = qobject_cast<QComboBox*>(object))
                ComboStyle::prepare(combo);
            // Dates read in the app language (numbers keep the decimal point).
            if (auto* dateEdit = qobject_cast<QDateEdit*>(object))
                dateEdit->setLocale(TranslationManager::instance().locale());
            else if (auto* calendar = qobject_cast<QCalendarWidget*>(object))
                calendar->setLocale(TranslationManager::instance().locale());
            break;
        case QEvent::Show:
            if (auto* dialog = qobject_cast<QDialog*>(object);
                dialog && !event->spontaneous() && fadesIn(dialog) && Animations::enabled())
                fadeInDialog(dialog);
            else if (object->inherits("QComboBoxPrivateContainer") && !event->spontaneous()) {
                // The list is laid out just after it is shown.
                auto* popup = static_cast<QWidget*>(object);
                QTimer::singleShot(0, popup, [popup]() { Animations::slideDownReveal(popup); });
            }
            break;
        default:
            break;
        }
        return false;
    }

private:
    static void onHoverChange(QWidget* widget, bool entering)
    {
        // Qt has already updated "under mouse": draw the look from just before.
        widget->setAttribute(Qt::WA_UnderMouse, !entering);
        const QPixmap previous = snapshotWithoutOverlays(widget);
        widget->setAttribute(Qt::WA_UnderMouse, entering);
        crossfade(widget, previous, HoverFadeMs, OverlayRole::Hover);
        if (entering && widget->property(GlareProperty).toBool())
            sweepGlare(widget);
    }

    static void onPress(QWidget* widget)
    {
        const QPixmap previous = snapshotWithoutOverlays(widget);
        crossfade(widget, previous, PressStateFadeMs, OverlayRole::PressState);
        // Tiles act on the press and may be rebuilt or covered by a dialog at
        // once, so their feedback is a self-contained pulse.
        startPressDim(widget, previous, isProductTile(widget));
    }

    static void onRelease(QWidget* widget)
    {
        Overlay* dim = findOverlay(widget, OverlayRole::PressDim);
        if (!dim)
            return;
        if (!isProductTile(widget))
            crossfade(widget, snapshotWithoutOverlays(widget), ReleaseFadeMs, OverlayRole::PressState);
        fadeOutPressDim(dim);
    }
};

class TabAnimator : public QObject
{
public:
    explicit TabAnimator(QTabWidget* tabs)
        : QObject(tabs)
        , m_tabs(tabs)
        , m_bar(tabs->tabBar())
        , m_current(tabs->currentIndex())
    {
        m_bar->installEventFilter(this);
        connect(tabs, &QTabWidget::currentChanged, this, &TabAnimator::onCurrentChanged);
    }

    bool eventFilter(QObject*, QEvent* event) override
    {
        // The press switches the tab, so this is the last look at the old page.
        if (event->type() == QEvent::MouseButtonPress && Animations::enabled()) {
            const int tab = m_bar->tabAt(static_cast<QMouseEvent*>(event)->position().toPoint());
            QWidget* page = m_tabs->currentWidget();
            if (tab >= 0 && tab != m_tabs->currentIndex() && page) {
                m_oldPage = snapshot(page);
                m_oldPageRect = QRect(page->mapTo(m_tabs, QPoint(0, 0)), page->size());
            }
        }
        return false;
    }

private:
    void onCurrentChanged(int index)
    {
        const int from = m_current;
        m_current = index;
        const QPixmap oldPage = m_oldPage;
        const QRect oldPageRect = m_oldPageRect;
        m_oldPage = QPixmap();
        if (oldPage.isNull() || from < 0 || index < 0 || !Animations::enabled())
            return;
        // After the app's own handlers, which may load data for the new page.
        QTimer::singleShot(0, this, [this, from, index, oldPage, oldPageRect] {
            slideHighlight(from, index);
            fadeOldPage(oldPage, oldPageRect);
        });
    }

    void slideHighlight(int from, int to)
    {
        const QRect a = m_bar->tabRect(from);
        const QRect b = m_bar->tabRect(to);
        const QColor accent = qApp->palette().color(QPalette::Highlight);
        auto* overlay = new Overlay(m_bar);
        auto rect = std::make_shared<QRectF>(a);
        auto opacity = std::make_shared<qreal>(1.0);
        overlay->paint = [rect, opacity, accent](QPainter& painter) {
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setOpacity(*opacity);
            painter.setPen(Qt::NoPen);
            painter.setBrush(accent);
            painter.drawRoundedRect(QRectF(rect->left() + 6, rect->bottom() - 3,
                                           rect->width() - 12, 3), 1.5, 1.5);
        };
        QPointer<Overlay> guard(overlay);
        tween(overlay, TabSlideMs, QEasingCurve::OutCubic,
              [guard, rect, a, b](qreal t) {
                  *rect = QRectF(a.left() + (b.left() - a.left()) * t, a.top(),
                                 a.width() + (b.width() - a.width()) * t, a.height());
                  if (guard) guard->update();
              },
              [guard, opacity] {
                  if (!guard) return;
                  tween(guard, TabHighlightFadeMs, QEasingCurve::Linear,
                        [guard, opacity](qreal t) { *opacity = 1.0 - t; if (guard) guard->update(); },
                        [guard] { removeWhenDone(guard); });
              });
    }

    void fadeOldPage(const QPixmap& oldPage, const QRect& rect)
    {
        auto* overlay = new Overlay(m_tabs, OverlayRole::Other, rect);
        auto opacity = std::make_shared<qreal>(1.0);
        overlay->paint = [oldPage, opacity](QPainter& painter) {
            painter.setOpacity(*opacity);
            painter.drawPixmap(0, 0, oldPage);
        };
        QPointer<Overlay> guard(overlay);
        tween(overlay, PageFadeMs, QEasingCurve::OutCubic,
              [guard, opacity](qreal t) { *opacity = 1.0 - t; if (guard) guard->update(); },
              [guard] { removeWhenDone(guard); });
    }

    QTabWidget* m_tabs;
    QTabBar* m_bar;
    int m_current;
    QPixmap m_oldPage;
    QRect m_oldPageRect;
};

} // namespace

namespace Animations {

void install()
{
    qApp->installEventFilter(new Controller(qApp));
}

bool enabled()
{
    return LocalSettings::load().animationsEnabled;
}

void enableGlare(QWidget* widget)
{
    widget->setProperty(GlareProperty, true);
}

void attachTabs(QTabWidget* tabs)
{
    new TabAnimator(tabs);
}

void slideDownReveal(QWidget* widget)
{
    if (!enabled() || widget->size().isEmpty())
        return;
    const QPixmap look = widget->grab();
    const QImage image = look.toImage();
    const QColor background = image.pixelColor(image.width() / 2, image.height() - 3);
    const int height = widget->height();
    auto* overlay = new Overlay(widget);
    auto y = std::make_shared<qreal>(-height);
    const qreal radius = widget->testAttribute(Qt::WA_TranslucentBackground) ? RoundedWindowRadius : 0;
    overlay->paint = [look, y, background, overlay, radius](QPainter& painter) {
        painter.setRenderHint(QPainter::Antialiasing);
        QPainterPath shape;
        shape.addRoundedRect(QRectF(overlay->rect()), radius, radius);
        painter.setClipPath(shape);
        painter.fillRect(overlay->rect(), background);
        painter.drawPixmap(QPointF(0, *y), look);
    };
    QPointer<Overlay> guard(overlay);
    tween(overlay, PanelSlideMs, QEasingCurve::OutCubic,
          [guard, y, height](qreal t) { *y = -height * (1.0 - t); if (guard) guard->update(); },
          [guard] { removeWhenDone(guard); });
}

void switchPage(QStackedWidget* stack, int index)
{
    QWidget* oldPage = stack->currentWidget();
    if (!enabled() || !oldPage || index == stack->currentIndex()) {
        stack->setCurrentIndex(index);
        return;
    }

    // A page still rising from an earlier switch goes straight to its place.
    const QPoint home = oldPage->pos().x() == stack->contentsRect().x()
                            ? stack->contentsRect().topLeft() : oldPage->pos();
    oldPage->move(home);
    const int generation = stack->property(GenerationProperty).toInt() + 1;
    stack->setProperty(GenerationProperty, generation);

    const QPixmap oldLook = snapshot(oldPage);
    const QRect oldRect = oldPage->geometry();
    stack->setCurrentIndex(index);
    QWidget* newPage = stack->currentWidget();

    auto* overlay = new Overlay(stack, OverlayRole::Other, oldRect);
    auto opacity = std::make_shared<qreal>(1.0);
    overlay->paint = [oldLook, opacity](QPainter& painter) {
        painter.setOpacity(*opacity);
        painter.drawPixmap(0, 0, oldLook);
    };

    QPointer<Overlay> guard(overlay);
    QPointer<QWidget> page(newPage);
    QPointer<QStackedWidget> owner(stack);
    tween(overlay, PageSwitchMs, QEasingCurve::OutCubic,
          [guard, opacity, page, owner, generation, home](qreal t) {
              *opacity = 1.0 - t;
              if (guard) guard->update();
              if (page && owner && owner->property(GenerationProperty).toInt() == generation)
                  page->move(home + QPoint(0, int(PageRiseOffset * (1.0 - t))));
          },
          [guard, page, home, owner, generation] {
              if (page && owner && owner->property(GenerationProperty).toInt() == generation)
                  page->move(home);
              removeWhenDone(guard);
          });
}

void fadeChange(QWidget* widget)
{
    if (!enabled() || !widget->isVisible() || widget->size().isEmpty())
        return;
    crossfade(widget, snapshotWithoutOverlays(widget), ContentFadeMs, OverlayRole::Other);
}

void rowIn(QWidget* row)
{
    if (!enabled())
        return;
    const int height = row->sizeHint().height();
    QPointer<QWidget> guard(row);
    tween(row, RowMs, QEasingCurve::OutCubic,
          [guard, height](qreal t) { if (guard) guard->setMaximumHeight(int(height * t)); },
          [guard] { if (guard) guard->setMaximumHeight(QWIDGETSIZE_MAX); });
}

QPixmap rowSnapshot(QWidget* row)
{
    return enabled() ? snapshot(row) : QPixmap();
}

void collapseRow(QBoxLayout* layout, int index, const QPixmap& picture)
{
    if (picture.isNull() || !enabled())
        return;
    auto* ghost = new QLabel(layout->parentWidget());
    ghost->setObjectName("posboxAnimRow");
    ghost->setAttribute(Qt::WA_TranslucentBackground);
    ghost->setAttribute(Qt::WA_TransparentForMouseEvents);
    ghost->setAlignment(Qt::AlignTop);
    ghost->setPixmap(picture);
    const int height = qRound(picture.height() / picture.devicePixelRatio());
    ghost->setMaximumHeight(height);
    layout->insertWidget(index, ghost);
    QPointer<QLabel> guard(ghost);
    tween(ghost, RowMs, QEasingCurve::InOutCubic,
          [guard, height](qreal t) { if (guard) guard->setMaximumHeight(int(height * (1.0 - t))); },
          [guard] { if (guard) guard->deleteLater(); });
}

void animateValues(QObject* owner, const QVector<double>& to,
                   const std::function<void(const QVector<double>&)>& apply)
{
    const QVariant previousTarget = owner->property(ValuesProperty);
    const QVector<double> shown = owner->property(ShownProperty).value<QVector<double>>();
    const int generation = owner->property(GenerationProperty).toInt() + 1;
    owner->setProperty(GenerationProperty, generation);  // stops any running count
    owner->setProperty(ValuesProperty, QVariant::fromValue(to));

    if (!previousTarget.isValid() || shown == to || shown.size() != to.size() || !enabled()) {
        owner->setProperty(ShownProperty, QVariant::fromValue(to));
        apply(to);
        return;
    }

    QPointer<QObject> guard(owner);
    tween(owner, ValueMs, QEasingCurve::OutCubic,
          [guard, generation, shown, to, apply](qreal t) {
              if (!guard || guard->property(GenerationProperty).toInt() != generation)
                  return;
              QVector<double> values(to.size());
              for (int i = 0; i < to.size(); ++i)
                  values[i] = shown[i] + (to[i] - shown[i]) * t;
              guard->setProperty(ShownProperty, QVariant::fromValue(values));
              apply(values);
          },
          [guard, generation, to, apply] {
              if (!guard || guard->property(GenerationProperty).toInt() != generation)
                  return;
              guard->setProperty(ShownProperty, QVariant::fromValue(to));
              apply(to);
          });
}

} // namespace Animations
