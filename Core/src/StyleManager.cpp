#include "StyleManager.h"
#include "CommonConfig.h"
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <QSqlQuery>
#include <QSqlDatabase>

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
// BACKGROUND SNIPPET
// Appended last so it always wins over whatever the color file set.
// ============================================================================

QString StyleManager::backgroundSnippet(Background bg)
{
    const char* path = nullptr;
    switch (bg) {
    case Background::Caramel: path = POSConfig::BG_PATH_CARAMEL; break;
    case Background::Black:   path = POSConfig::BG_PATH_BLACK;   break;
    case Background::Blue:    path = POSConfig::BG_PATH_BLUE;    break;
    default:                  path = POSConfig::BG_PATH_DEFAULT;  break;
    }

    return QString(
               "\n/* ── StyleManager background override ── */\n"
               "QWidget#centralwidget {\n"
               "    background-image: url(%1);\n"
               "    background-position: center;\n"
               "    background-repeat: no-repeat;\n"
               "    background-attachment: fixed;\n"
               "}\n"
               ).arg(path);
}

// ============================================================================
// LOAD STYLESHEET (color + font + background)
// ============================================================================

QString StyleManager::loadStyleSheet(ColorScheme color, FontSize font, Background bg)
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

    // ── Concatenate: color + font + background snippet ────────────────────────
    return colorCache + "\n" + *fontCache + backgroundSnippet(bg);
}

// ============================================================================
// APPLY
// ============================================================================

void StyleManager::applyGlobalStyles(QWidget* widget)
{
    applyGlobalStyles(widget, QSqlDatabase::database());
}

void StyleManager::applyGlobalStyles(QWidget* widget, const QSqlDatabase& db)
{
    if (!widget) return;
    const QString sheet = loadStyleSheet(loadColorScheme(db), loadFontSize(db), loadBackground(db));
    if (!sheet.isEmpty())
        widget->setStyleSheet(sheet);
}

void StyleManager::applyToAllWindows()
{
    applyToAllWindows(QSqlDatabase::database());
}

void StyleManager::applyToAllWindows(const QSqlDatabase& db)
{
    if (!qApp) return;
    const QString sheet = loadStyleSheet(loadColorScheme(db), loadFontSize(db), loadBackground(db));
    if (sheet.isEmpty()) return;
    for (QWidget* w : qApp->topLevelWidgets())
        w->setStyleSheet(sheet);
}

// ============================================================================
// SETTINGS I/O — database helpers
// ============================================================================

static QString getSettingFromDb(const QString& key, const QSqlDatabase& db)
{
    QSqlQuery query(db);
    query.exec("SELECT " + key + " FROM app_settings WHERE id = 1");
    if (query.next()) {
        return query.value(0).toString();
    }
    return QString();
}

static void saveSettingToDb(const QString& key, const QString& value, const QSqlDatabase& db)
{
    QSqlQuery query(db);
    query.prepare("UPDATE app_settings SET " + key + " = :value WHERE id = 1");
    query.bindValue(":value", value);
    query.exec();
}

// ============================================================================
// COLOR SCHEME
// ============================================================================

StyleManager::ColorScheme StyleManager::loadColorScheme()
{
    return loadColorScheme(QSqlDatabase::database());
}

StyleManager::ColorScheme StyleManager::loadColorScheme(const QSqlDatabase& db)
{
    const QString v = getSettingFromDb("color_scheme", db);
    return (v == POSConfig::COLOR_SCHEME_LIGHT) ? ColorScheme::Light : ColorScheme::Dark;
}

void StyleManager::saveColorScheme(ColorScheme color)
{
    saveColorScheme(color, QSqlDatabase::database());
}

void StyleManager::saveColorScheme(ColorScheme color, const QSqlDatabase& db)
{
    QString value = (color == ColorScheme::Light)
                        ? POSConfig::COLOR_SCHEME_LIGHT
                        : POSConfig::COLOR_SCHEME_DARK;
    saveSettingToDb("color_scheme", value, db);
    s_cachedDark.clear();
    s_cachedLight.clear();
    qDebug() << "StyleManager: color ->" << value;
}

// ============================================================================
// FONT SIZE
// ============================================================================

StyleManager::FontSize StyleManager::loadFontSize()
{
    return loadFontSize(QSqlDatabase::database());
}

StyleManager::FontSize StyleManager::loadFontSize(const QSqlDatabase& db)
{
    const QString v = getSettingFromDb("font_size", db);
    if (v == POSConfig::FONT_SIZE_MEDIUM) return FontSize::Medium;
    if (v == POSConfig::FONT_SIZE_LARGE)  return FontSize::Large;
    return FontSize::Small;
}

void StyleManager::saveFontSize(FontSize font)
{
    saveFontSize(font, QSqlDatabase::database());
}

void StyleManager::saveFontSize(FontSize font, const QSqlDatabase& db)
{
    QString value;
    switch (font) {
    case FontSize::Medium: value = POSConfig::FONT_SIZE_MEDIUM; break;
    case FontSize::Large:  value = POSConfig::FONT_SIZE_LARGE;  break;
    default:               value = POSConfig::FONT_SIZE_SMALL;  break;
    }
    saveSettingToDb("font_size", value, db);
    s_cachedFontSmall.clear();
    s_cachedFontMedium.clear();
    s_cachedFontLarge.clear();
    qDebug() << "StyleManager: font ->" << value;
}

// ============================================================================
// BACKGROUND
// ============================================================================

StyleManager::Background StyleManager::loadBackground()
{
    return loadBackground(QSqlDatabase::database());
}

StyleManager::Background StyleManager::loadBackground(const QSqlDatabase& db)
{
    const QString v = getSettingFromDb("background", db);
    if (v == POSConfig::BG_CARAMEL) return Background::Caramel;
    if (v == POSConfig::BG_BLACK)   return Background::Black;
    if (v == POSConfig::BG_BLUE)    return Background::Blue;
    return Background::Default;
}

void StyleManager::saveBackground(Background bg)
{
    saveBackground(bg, QSqlDatabase::database());
}

void StyleManager::saveBackground(Background bg, const QSqlDatabase& db)
{
    QString value;
    switch (bg) {
    case Background::Caramel: value = POSConfig::BG_CARAMEL; break;
    case Background::Black:   value = POSConfig::BG_BLACK;   break;
    case Background::Blue:    value = POSConfig::BG_BLUE;    break;
    default:                  value = POSConfig::BG_DEFAULT;  break;
    }
    saveSettingToDb("background", value, db);
    qDebug() << "StyleManager: background ->" << value;
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
