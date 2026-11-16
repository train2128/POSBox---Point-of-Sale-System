#ifndef STYLEMANAGER_H
#define STYLEMANAGER_H

#include <QString>
#include <QWidget>

/**
 * Manages application-wide stylesheets.
 * Loads the color theme (dark/light) and appends the font-size layer.
 */
class StyleManager
{
public:
    enum class ColorScheme { Dark, Light };
    enum class FontSize    { Small, Medium, Large };

    /**
     * Load and concatenate color + font into one stylesheet.
     */
    static QString loadStyleSheet(ColorScheme color, FontSize font);

    /**
     * Apply the saved theme (from the local settings file) to a single widget.
     */
    static void applySavedTheme(QWidget* widget);

    /**
     * Apply an explicit theme.
     */
    static void applyTheme(QWidget* widget, ColorScheme color, FontSize font);

    static ColorScheme loadColorScheme();
    static FontSize    loadFontSize();

    /**
     * Return the full resource path for a named icon.
     * Automatically picks the white variant for Dark theme,
     * or the _black variant for Light theme.
     * e.g. iconPath("logout") -> ":/Assets/images/Assets/logout.png"
     *                         or ":/Assets/images/Assets/logout_black.png"
     */
    static QString iconPath(const QString& name);

    /**
     * Background tint (Light theme): a #RRGGBB color stored in the local
     * settings that replaces the default caramel background with a native
     * radial "light spot" gradient derived from it. Empty = theme default.
     */
    static QString customBackgroundColor();

    /**
     * Background image (user-uploaded, stored in local settings).
     * QSS can't scale an image, so this installs a QLabel behind the window
     * content and keeps it in sync on resize. scaleMode selects the fill:
     *   "cover"   (default) - scale up until it fills side to side, centered,
     *                         overflow cropped (aspect preserved)
     *   "stretch"           - exact fill of the window (may distort)
     *   "fit"               - fully contained, centered (gradient shows around)
     * dim (0-100) paints a black scrim into the image for legibility.
     * An empty path — or a missing/invalid file — removes any installed image
     * so the theme gradient (or the color tint) shows through instead.
     */
    static void installBackground(QWidget* widget, const QString& imagePath,
                                  const QString& scaleMode = QString(),
                                  int dim = 0);

private:
    StyleManager() = delete;
    ~StyleManager() = delete;

    static QString loadFile(const QString& resourcePath);
    static QString backgroundOverrideQss(const QString& hex);

    static QString s_cachedDark;
    static QString s_cachedLight;
    static QString s_cachedFontSmall;
    static QString s_cachedFontMedium;
    static QString s_cachedFontLarge;
};

#endif // STYLEMANAGER_H
