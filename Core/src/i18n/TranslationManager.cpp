#include "TranslationManager.h"
#include <QApplication>
#include <QTranslator>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlDatabase>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QLocale>
#include <QStandardPaths>

// Each language is shown in its own script, so anyone can find theirs.
const QList<TranslationManager::LanguageInfo> TranslationManager::LANGUAGES = {
    {"en", "English"},
    {"fr", QString::fromUtf8("Français")},
    {"es", QString::fromUtf8("Español")},
    {"pt", QString::fromUtf8("Português")},
    {"de", QString::fromUtf8("Deutsch")},
    {"it", QString::fromUtf8("Italiano")},
    {"tr", QString::fromUtf8("Türkçe")},
    {"ru", QString::fromUtf8("Русский")},
    {"ar", QString::fromUtf8("العربية")},
    {"zh", QString::fromUtf8("中文 (简体)")},
};

namespace {

// Qt's own translations (file dialogs, color picker, standard buttons).
QString qtBaseFile(const QString& langCode)
{
    if (langCode == "pt") return QStringLiteral("qtbase_pt_BR");
    if (langCode == "zh") return QStringLiteral("qtbase_zh_CN");
    return QStringLiteral("qtbase_") + langCode;
}

} // namespace

TranslationManager& TranslationManager::instance()
{
    static TranslationManager mgr;
    return mgr;
}

TranslationManager::TranslationManager(QObject* parent)
    : QObject(parent)
    , m_translator(nullptr)
    , m_qtTranslator(nullptr)
    , m_currentLang("en")
    , m_initialized(false)
{
}

TranslationManager::~TranslationManager()
{
    if (m_translator) {
        qApp->removeTranslator(m_translator);
        delete m_translator;
    }
    if (m_qtTranslator) {
        qApp->removeTranslator(m_qtTranslator);
        delete m_qtTranslator;
    }
}

void TranslationManager::init(QSqlDatabase db)
{
    m_db = db;
    m_initialized = true;
    loadFromDatabase();
}

void TranslationManager::loadFromDatabase()
{
    if (!m_initialized) return;

    QSqlQuery query(m_db);
    query.prepare("SELECT language FROM app_settings WHERE id = 1");
    if (query.exec() && query.next()) {
        QString lang = query.value(0).toString().trimmed();
        if (!lang.isEmpty() && lang != m_currentLang) {
            loadLanguage(lang);
            return;
        }
    }
    // Default is already "en"
}

void TranslationManager::saveToDatabase(const QString& langCode)
{
    if (!m_initialized) return;

    QSqlQuery query(m_db);
    query.prepare("UPDATE app_settings SET language = ? WHERE id = 1");
    query.addBindValue(langCode);
    if (!query.exec()) {
        qWarning() << "TranslationManager: failed to save language:" << query.lastError().text();
    } else {
        qDebug() << "TranslationManager: saved language" << langCode << "to database";
    }
}

void TranslationManager::loadLanguage(const QString& requested)
{
    // Unknown codes (an old or edited database) fall back to English.
    QString langCode = QStringLiteral("en");
    for (const auto& lang : LANGUAGES) {
        if (lang.code == requested)
            langCode = requested;
    }

    // Always save to DB first
    saveToDatabase(langCode);

    if (langCode == m_currentLang) return;

    // Remove old translators
    for (QTranslator** translator : {&m_translator, &m_qtTranslator}) {
        if (*translator) {
            qApp->removeTranslator(*translator);
            delete *translator;
            *translator = nullptr;
        }
    }

    m_currentLang = langCode;

    // Arabic reads right to left: the whole interface is mirrored.
    if (qApp)
        qApp->setLayoutDirection(langCode == "ar" ? Qt::RightToLeft : Qt::LeftToRight);

    // English is the base language — no .qm file needed
    if (langCode == "en") {
        emit languageChanged(langCode);
        return;
    }

    // Built into the apps; a translations folder next to them is the fallback.
    QString qmFile = ":/translations/posbox_" + langCode + ".qm";
    if (!QFile::exists(qmFile))
        qmFile = QCoreApplication::applicationDirPath() + "/translations/posbox_" + langCode + ".qm";

    if (!QFile::exists(qmFile)) {
        qWarning() << "TranslationManager: .qm file not found for" << langCode;
        emit languageChanged(langCode);
        return;
    }

    m_translator = new QTranslator(this);
    if (m_translator->load(qmFile)) {
        qApp->installTranslator(m_translator);
    } else {
        qWarning() << "TranslationManager: failed to load" << qmFile;
        delete m_translator;
        m_translator = nullptr;
    }

    m_qtTranslator = new QTranslator(this);
    if (m_qtTranslator->load(":/translations/" + qtBaseFile(langCode) + ".qm")) {
        qApp->installTranslator(m_qtTranslator);
    } else {
        delete m_qtTranslator;
        m_qtTranslator = nullptr;
    }

    emit languageChanged(langCode);
}

QLocale TranslationManager::locale() const
{
    // Names of months and days in the app language. Numbers keep the
    // application default (decimal point), see AppRuntime::configureProcess.
    static const QHash<QString, QLocale> locales = {
        {"fr", QLocale(QLocale::French, QLocale::France)},
        {"es", QLocale(QLocale::Spanish, QLocale::Spain)},
        {"pt", QLocale(QLocale::Portuguese, QLocale::Brazil)},
        {"de", QLocale(QLocale::German, QLocale::Germany)},
        {"it", QLocale(QLocale::Italian, QLocale::Italy)},
        {"tr", QLocale(QLocale::Turkish, QLocale::Turkey)},
        {"ru", QLocale(QLocale::Russian, QLocale::Russia)},
        {"ar", QLocale(QLocale::Arabic, QLocale::SaudiArabia)},
        {"zh", QLocale(QLocale::Chinese, QLocale::China)},
    };
    return locales.value(m_currentLang, QLocale(QLocale::English, QLocale::UnitedStates));
}

QString TranslationManager::currentLanguage() const
{
    return m_currentLang;
}

QList<TranslationManager::LanguageInfo> TranslationManager::availableLanguages() const
{
    return LANGUAGES;
}

QString TranslationManager::languageName(const QString& langCode) const
{
    for (const auto& lang : LANGUAGES) {
        if (lang.code == langCode)
            return lang.name;
    }
    return langCode;
}
