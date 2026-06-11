#ifndef TRANSLATIONMANAGER_H
#define TRANSLATIONMANAGER_H

#include <QObject>
#include <QMap>
#include <QStringList>
#include <QSqlDatabase>

class QTranslator;

class TranslationManager : public QObject
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
    QString m_currentLang;
    QSqlDatabase m_db;
    bool m_initialized;

    static const QList<LanguageInfo> LANGUAGES;
};

#endif // TRANSLATIONMANAGER_H
