#include "TranslationManager.h"
#include "QueryRepository.h"
#include <QApplication>
#include <QTranslator>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlDatabase>
#include <QDebug>
#include <QDir>
#include <QStandardPaths>

const QList<TranslationManager::LanguageInfo> TranslationManager::LANGUAGES = {
    {"en", "English"},
    {"fr", "Francais"},
};

TranslationManager& TranslationManager::instance()
{
    static TranslationManager mgr;
    return mgr;
}

TranslationManager::TranslationManager(QObject* parent)
    : QObject(parent)
    , m_translator(nullptr)
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
    query.prepare(Queries::APP_SETTINGS_UPDATE_LANGUAGE);
    query.addBindValue(langCode);
    if (!query.exec()) {
        qWarning() << "TranslationManager: failed to save language:" << query.lastError().text();
    } else {
        qDebug() << "TranslationManager: saved language" << langCode << "to database";
    }
}

void TranslationManager::loadLanguage(const QString& langCode)
{
    // Always save to DB first
    saveToDatabase(langCode);

    if (langCode == m_currentLang) return;

    // Remove old translator
    if (m_translator) {
        qApp->removeTranslator(m_translator);
        delete m_translator;
        m_translator = nullptr;
    }

    m_currentLang = langCode;

    // English is the base language — no .qm file needed
    if (langCode == "en") {
        emit languageChanged(langCode);
        return;
    }

    // Look for .qm file in translations directory
    QString translationsPath = QCoreApplication::applicationDirPath()
                               + "/translations";

    QString qmFile = translationsPath + "/posbox_" + langCode + ".qm";

    if (!QFile::exists(qmFile)) {
        // Also check Resources path
        qmFile = ":/translations/posbox_" + langCode + ".qm";
    }

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

    emit languageChanged(langCode);
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
