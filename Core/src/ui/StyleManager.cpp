#include "StyleManager.h"
#include "CommonConfig.h"
#include "LocalSettings.h"
#include <QFile>
#include <QTextStream>
#include <QApplication>
#include <QDebug>
#include <QColor>
#include <QMainWindow>
#include <QLabel>
#include <QPixmap>
#include <QPainter>
#include <QVariant>
#include <QEvent>
#include <QPalette>

// Static cache
QString StyleManager::s_cachedDark;
QString StyleManager::s_cachedLight;
QString StyleManager::s_cachedFontSmall;
QString StyleManager::s_cachedFontMedium;
QString StyleManager::s_cachedFontLarge;

// ============================================================================
// INTERNAL FILE LOADER
// ============================================================================

QString StyleManager::loadFile(const QString& resourcePath)
{
    QFile file(resourcePath);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        qWarning() << "StyleManager: failed to load:" << resourcePath;
        return QString();
    }
    const QString content = QTextStream(&file).readAll();
    qDebug() << "StyleManager: loaded" << resourcePath << "(" << content.length() << "chars)";
    return content;
}

// ============================================================================
// LOAD STYLESHEET (color + font)
// ============================================================================

QString StyleManager::loadStyleSheet(ColorScheme color, FontSize font)
{

    // ── Color file ───────────────────────────────────────────────────────────
    QString& colorCache = (color == ColorScheme::Light) ? s_cachedLight : s_cachedDark;
    if (colorCache.isEmpty()) {
        colorCache = loadFile(color == ColorScheme::Light
                                  ? ":/styles/styles/app_styles_light.qss"
                                  : ":/styles/styles/app_styles_dark.qss");
    }

    // ── Font file ────────────────────────────────────────────────────────────
    QString* fontCache = nullptr;
    QString  fontPath;
    switch (font) {
    case FontSize::Medium:
        fontCache = &s_cachedFontMedium;
        fontPath  = ":/styles/styles/app_styles_font_medium.qss";
        break;
    case FontSize::Large:
        fontCache = &s_cachedFontLarge;
        fontPath  = ":/styles/styles/app_styles_font_large.qss";
        break;
    default:
        fontCache = &s_cachedFontSmall;
        fontPath  = ":/styles/styles/app_styles_font_small.qss";
        break;
    }
    if (fontCache->isEmpty())
        *fontCache = loadFile(fontPath);

    // ── Concatenate: color + font ─────────────────────────────────────────────
    QString sheet = colorCache + "\n" + *fontCache;

    // Custom background tint (Light theme only): replace the default caramel
    // background with a native radial "light spot" gradient derived from the
    // user-chosen color. Appended last so it overrides the base rule.
    if (color == ColorScheme::Light) {
        const QString bg = customBackgroundColor();
        if (!bg.isEmpty())
            sheet += "\n" + backgroundOverrideQss(bg);
    }
    return sheet;
}

// ============================================================================
// BACKGROUND TINT (Light theme)
// ============================================================================

QString StyleManager::customBackgroundColor()
{
    return LocalSettings::load().backgroundColor.trimmed();
}

// Build a radial "light spot" background from a user-picked hex color. The
// color is clamped to a light range so the espresso light-theme text stays
// legible no matter what the user picks, while preserving their hue.
QString StyleManager::backgroundOverrideQss(const QString& hex)
{
    QColor base(hex);
    if (!base.isValid()) return QString();

    int h = 0, s = 0, l = 0;
    base.getHsl(&h, &s, &l);
    if (h < 0) h = 0;
    if (l < 158) l = 158;   // keep it light (~0.62) for legibility
    if (l > 242) l = 242;   // ...but not pure white (~0.95)

    const QColor clamped = QColor::fromHsl(h, s, l);
    const QColor light   = clamped.lighter(126);
    const QColor edge    = clamped.darker(110);
    const QColor dialog  = clamped.lighter(148);

    return QStringLiteral(
               "QMainWindow { background: qradialgradient(cx:0.30, cy:0.18, radius:1.25,"
               " stop:0 %1, stop:0.5 %2, stop:1 %3); }\n"
               // The launcher/login window (a QDialog) must show the same tint,
               // not the flat dialog color — id rule beats the generic QDialog
               // rule below, matching the base-theme #launcherCard rule.
               "QFrame#launcherCard { background: qradialgradient(cx:0.30, cy:0.18,"
               " radius:1.25, stop:0 %1, stop:0.5 %2, stop:1 %3); }\n"
               "QDialog { background-color: %4; }")
        .arg(light.name().toUpper(),
             clamped.name().toUpper(),
             edge.name().toUpper(),
             dialog.name().toUpper());
}

// ============================================================================
// BACKGROUND IMAGE
// QSS can't scale a background image, so we install a QLabel behind the window
// content. The image is scaled to FILL the window (cover / stretch / fit,
// see installBackground), optionally with a dim scrim, and re-scaled whenever
// the window resizes.
// ============================================================================
namespace {

// Rebuild the label's pixmap for `target` using the mode/dim stored on it as
// dynamic properties, and size the label to the full target rect.
void scaleBackground(QLabel* label, const QSize& target)
{
    const QPixmap orig = label->property("posboxBgOrig").value<QPixmap>();
    if (orig.isNull() || target.isEmpty())
        return;

    const QString mode = label->property("posboxBgScale").toString();
    QPixmap scaled;
    if (mode == QLatin1String("stretch")) {
        scaled = orig.scaled(target, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    } else {
        // cover = max ratio (fill, crop overflow), fit = min ratio (contain)
        const qreal sw = qreal(target.width())  / orig.width();
        const qreal sh = qreal(target.height()) / orig.height();
        const qreal s  = (mode == QLatin1String("fit")) ? qMin(sw, sh) : qMax(sw, sh);
        scaled = orig.scaled(qRound(orig.width() * s), qRound(orig.height() * s),
                             Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    // Dim scrim: composed into the pixmap so there is never a second widget
    // to stack or resize (label stays the single background layer).
    const int dim = label->property("posboxBgDim").toInt();
    if (dim > 0) {
        QPainter p(&scaled);
        p.fillRect(scaled.rect(), QColor(0, 0, 0, qBound(0, dim, 100) * 255 / 100));
        p.end();
    }

    label->setPixmap(scaled);
    label->setGeometry(QRect(QPoint(0, 0), target));
}

// Keeps the scaled background label matched to its parent window.
class BackgroundResizer : public QObject {
public:
    using QObject::QObject;
protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() == QEvent::Resize) {
            if (auto* label = watched->findChild<QLabel*>("posboxScaledBackground")) {
                if (auto* top = qobject_cast<QWidget*>(watched))
                    scaleBackground(label, top->size());
            }
        }
        return QObject::eventFilter(watched, event);
    }
};

BackgroundResizer* bgResizer()
{
    static BackgroundResizer instance;
    return &instance;
}

} // namespace

void StyleManager::installBackground(QWidget* widget, const QString& imagePath,
                                     const QString& scaleMode, int dim)
{
    if (!widget)
        return;

    // Drop any previously installed image first (also clears the resizer).
    // Synchronous delete: the label is only ever replaced from apply paths,
    // never from its own event handlers, and this avoids a frame where the
    // old image lingers above the new one / the fallback.
    if (auto* old = widget->findChild<QLabel*>("posboxScaledBackground")) {
        widget->removeEventFilter(bgResizer());
        delete old;
    }

    if (imagePath.isEmpty())
        return;                          // no image -> gradient / color tint
    const QPixmap pix(imagePath);
    if (pix.isNull())
        return;                          // missing file -> gradient / color tint

    auto* bgLabel = new QLabel(widget);
    bgLabel->setObjectName("posboxScaledBackground");
    bgLabel->setProperty("posboxBgOrig", QVariant::fromValue(pix));
    bgLabel->setProperty("posboxBgScale",
                         scaleMode.isEmpty() ? QStringLiteral("cover") : scaleMode);
    bgLabel->setProperty("posboxBgDim", qBound(0, dim, 100));
    bgLabel->setAlignment(Qt::AlignCenter);
    bgLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    scaleBackground(bgLabel, widget->size());
    bgLabel->show();                     // needed when installed after show()
                                          // (Settings live re-apply path)
    bgLabel->lower();                    // behind every window child
    widget->installEventFilter(bgResizer());
}

// ============================================================================
// APPLY
// ============================================================================

// Install the saved background image — main windows only. (The launcher is a
// QDialog and installs its own copy explicitly in its constructor; plain
// dialogs keep their solid backgrounds.)
static void applySavedBackground(QWidget* w, const LocalSettings& s)
{
    if (qobject_cast<QMainWindow*>(w))
        StyleManager::installBackground(w, s.backgroundImage,
                                        s.backgroundScale, s.backgroundDim);
}

// A theme-matched application palette. Stylesheets win wherever they set a
// color, but widget styles paint surfaces QSS leaves alone (scroll-area
// viewports, tab panes, popups) from the palette.
namespace {

struct ThemeColors {
    const char* window;
    const char* text;
    const char* base;
    const char* alternateBase;
    const char* button;
    const char* brightText;
    const char* highlight;
    const char* placeholder;
    const char* light;
    const char* mid;
    const char* dark;
    const char* shadow;
    const char* disabledText;
    const char* disabledHighlight;
};

// Matches app_styles_dark.qss.
constexpr ThemeColors DarkColors = {
    "#1E2634", "#E6EAF0", "#232B3B", "#1B2331", "#212A3A", "#FF6B6B", "#4A90E2",
    "#8A94A6", "#2C3648", "#3A465C", "#121724", "#0B0F18", "#6B7484", "#2E3A4D",
};

// Matches app_styles_light.qss.
constexpr ThemeColors LightColors = {
    "#F1E7D5", "#40331F", "#FBF4E6", "#F6ECD6", "#F6ECD6", "#C0392B", "#C8894A",
    "#8B7B62", "#FFF9EE", "#D9C6A8", "#B9A17F", "#8B7B62", "#9B8E76", "#DCC9AC",
};

void applyPalette(StyleManager::ColorScheme scheme)
{
    if (!qApp) return;

    const ThemeColors& c =
        (scheme == StyleManager::ColorScheme::Dark) ? DarkColors : LightColors;

    QPalette p;
    p.setColor(QPalette::Window,          QColor(c.window));
    p.setColor(QPalette::WindowText,      QColor(c.text));
    p.setColor(QPalette::Base,            QColor(c.base));
    p.setColor(QPalette::AlternateBase,   QColor(c.alternateBase));
    p.setColor(QPalette::Text,            QColor(c.text));
    p.setColor(QPalette::Button,          QColor(c.button));
    p.setColor(QPalette::ButtonText,      QColor(c.text));
    p.setColor(QPalette::BrightText,      QColor(c.brightText));
    p.setColor(QPalette::Highlight,       QColor(c.highlight));
    p.setColor(QPalette::HighlightedText, QColor("#FFFFFF"));
    p.setColor(QPalette::PlaceholderText, QColor(c.placeholder));
    p.setColor(QPalette::ToolTipBase,     QColor(c.base));
    p.setColor(QPalette::ToolTipText,     QColor(c.text));
    p.setColor(QPalette::Link,            QColor(c.highlight));
    p.setColor(QPalette::LinkVisited,     QColor(c.highlight));
    p.setColor(QPalette::Light,           QColor(c.light));
    p.setColor(QPalette::Mid,             QColor(c.mid));
    p.setColor(QPalette::Dark,            QColor(c.dark));
    p.setColor(QPalette::Shadow,          QColor(c.shadow));

    p.setColor(QPalette::Disabled, QPalette::WindowText, QColor(c.disabledText));
    p.setColor(QPalette::Disabled, QPalette::Text,       QColor(c.disabledText));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(c.disabledText));
    p.setColor(QPalette::Disabled, QPalette::Highlight,  QColor(c.disabledHighlight));

    qApp->setPalette(p);
}

} // namespace

void StyleManager::applySavedTheme(QWidget* widget)
{
    applyTheme(widget, loadColorScheme(), loadFontSize());
}

void StyleManager::applyTheme(QWidget* widget, ColorScheme color, FontSize font)
{
    if (!widget) return;
    applyPalette(color);
    const QString sheet = loadStyleSheet(color, font);
    if (!sheet.isEmpty())
        widget->setStyleSheet(sheet);
    applySavedBackground(widget, LocalSettings::load());
}

StyleManager::ColorScheme StyleManager::loadColorScheme()
{
    return (LocalSettings::load().colorScheme == POSConfig::COLOR_SCHEME_LIGHT)
               ? ColorScheme::Light
               : ColorScheme::Dark;
}

StyleManager::FontSize StyleManager::loadFontSize()
{
    const QString v = LocalSettings::load().fontSize;
    if (v == "small") return FontSize::Small;
    if (v == "large") return FontSize::Large;
    return FontSize::Medium;
}

// ============================================================================
// ICON PATH
// ============================================================================

QString StyleManager::iconPath(const QString& name)
{
    const bool isLight = (loadColorScheme() == ColorScheme::Light);
    const QString suffix = isLight ? "_black.png" : ".png";
    return QString(":/Assets/images/Assets/") + name + suffix;
}
