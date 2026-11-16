#ifndef TRANSLATIONMANAGER_H
#define TRANSLATIONMANAGER_H

#include <QObject>
#include <QMap>
#include <QStringList>
#include <QSqlDatabase>
#include <QLocale>
#include "CoreExport.h"

class QTranslator;

class CORE_EXPORT TranslationManager : public QObject
{
    Q_OBJECT

public:
    struct LanguageInfo {
        QString code;       // e.g. "en", "fr"
        QString name;       // e.g. "English", "Francais"
    };

    static TranslationManager& instance();

    void init(QSqlDatabase db);
    void loadLanguage(const QString& langCode);
    QString currentLanguage() const;
    // Month and day names in the app language.
    QLocale locale() const;
    QList<LanguageInfo> availableLanguages() const;
    QString languageName(const QString& langCode) const;

signals:
    void languageChanged(const QString& langCode);

private:
    explicit TranslationManager(QObject* parent = nullptr);
    ~TranslationManager();

    void loadFromDatabase();
    void saveToDatabase(const QString& langCode);

    QTranslator* m_translator;
    QTranslator* m_qtTranslator;   // Qt's own dialogs and buttons
    QString m_currentLang;
    QSqlDatabase m_db;
    bool m_initialized;

    static const QList<LanguageInfo> LANGUAGES;
};

#endif // TRANSLATIONMANAGER_H
