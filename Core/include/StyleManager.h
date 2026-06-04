#ifndef STYLEMANAGER_H
#define STYLEMANAGER_H

#include <QString>
#include <QWidget>
#include <QSqlDatabase>

/**
 * Manages application-wide stylesheets.
 * Loads color + font QSS files and appends a background override snippet.
 */
class StyleManager
{
public:
    enum class ColorScheme { Dark, Light };
    enum class FontSize    { Small, Medium, Large };
    enum class Background  { Default, Caramel, Black, Blue };

    /**
     * Load and concatenate color + font + background into one stylesheet.
     */
    static QString loadStyleSheet(ColorScheme color, FontSize font, Background bg);

    /**
     * Apply the saved theme to a single widget.
     * Signature unchanged — all existing call sites continue to work.
     */
    static void applyGlobalStyles(QWidget* widget);
    static void applyGlobalStyles(QWidget* widget, const QSqlDatabase& db);

    /**
     * Re-apply the current saved theme to every top-level window.
     */
    static void applyToAllWindows();
    static void applyToAllWindows(const QSqlDatabase& db);

    // ── Load ─────────────────────────────────────────────────────────────────
    static ColorScheme loadColorScheme();
    static ColorScheme loadColorScheme(const QSqlDatabase& db);
    static FontSize    loadFontSize();
    static FontSize    loadFontSize(const QSqlDatabase& db);
    static Background  loadBackground();
    static Background  loadBackground(const QSqlDatabase& db);

    // ── Save ─────────────────────────────────────────────────────────────────
    static void saveColorScheme(ColorScheme color);
    static void saveColorScheme(ColorScheme color, const QSqlDatabase& db);
    static void saveFontSize(FontSize font);
    static void saveFontSize(FontSize font, const QSqlDatabase& db);
    static void saveBackground(Background bg);
    static void saveBackground(Background bg, const QSqlDatabase& db);

    /**
     * Return the full resource path for a named icon.
     * Automatically picks the white variant for Dark theme,
     * or the _black variant for Light theme.
     * e.g. iconPath("logout") -> ":/Assets/images/Assets/logout.png"
     *                         or ":/Assets/images/Assets/logout_black.png"
     */
    static QString iconPath(const QString& name);

private:
    StyleManager() = delete;
    ~StyleManager() = delete;

    static QString loadFile(const QString& resourcePath);
    static QString backgroundSnippet(Background bg);

    static QString s_cachedDark;
    static QString s_cachedLight;
    static QString s_cachedFontSmall;
    static QString s_cachedFontMedium;
    static QString s_cachedFontLarge;
};

#endif // STYLEMANAGER_H
