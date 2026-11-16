#include "Prefixes.h"
#include "MessageDialog.h"
#include <QDateTime>
#include <QCoreApplication>
#include <QObject>
#include <QFile>
#include <QLocale>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlQuery>

namespace AppConstants {
const char* CURRENCY = "TND";
int PRICE_DECIMALS = 3;
}

namespace AppHelpers {

QString roleName(const QString& role)
{
    static const char* const names[] = {
        QT_TRANSLATE_NOOP("Messages", "Admin"),
        QT_TRANSLATE_NOOP("Messages", "Manager"),
        QT_TRANSLATE_NOOP("Messages", "Cashier"),
    };
    for (const char* name : names) {
        if (role.compare(QLatin1String(name), Qt::CaseInsensitive) == 0)
            return QCoreApplication::translate("Messages", name);
    }
    return role;
}

void loadCurrencyConfig(QSqlDatabase db)
{
    if (!db.isValid() || !db.isOpen()) return;

    QSqlQuery query(db);
    query.prepare("SELECT currency FROM app_settings WHERE id = 1");
    if (query.exec() && query.next()) {
        const QString cur = query.value(0).toString().trimmed();
        if (!cur.isEmpty()) {
            static QByteArray storage;
            storage = cur.toUtf8();
            AppConstants::CURRENCY = storage.constData();
        }
    }

    // Decimal places follow the currency (ISO 4217 minor units).
    static const QSet<QString> zeroDecimals = {
        "JPY", "KRW", "VND", "UGX", "IDR", "RWF", "XAF", "XOF", "GNF",
        "CLP", "COP", "PYG", "MGA", "MMK", "TWD", "IRR", "VES",
        "DZD", "ISK", "DJF", "BIF", "KHR", "LAK", "MRO"
    };
    static const QSet<QString> threeDecimals = {
        "TND", "BHD", "KWD", "OMR", "JOD", "LYD", "IQD"
    };
    const QString curUpper = QString::fromUtf8(AppConstants::CURRENCY).toUpper();
    if (threeDecimals.contains(curUpper))
        AppConstants::PRICE_DECIMALS = 3;
    else if (zeroDecimals.contains(curUpper))
        AppConstants::PRICE_DECIMALS = 0;
    else
        AppConstants::PRICE_DECIMALS = 2;
}

QString formatPrice(double price)
{
    return QString("%1 %2")
    .arg(price, 0, 'f', AppConstants::PRICE_DECIMALS)
        .arg(AppConstants::CURRENCY);
}

QString formatAmount(double amount)
{
    return QLocale(QLocale::English).toString(amount, 'f', AppConstants::PRICE_DECIMALS);
}

QString formatDate(const QDate& date)
{
    return date.toString(AppConstants::DATETIME_DISPLAY).split(' ')[0];
}

bool confirmAction(QWidget* parent, const QString& message, const QString& title)
{
    // "Delete this ticket? This can't be undone." reads best split in two.
    const qsizetype questionEnd = message.indexOf('?');
    const QString heading = questionEnd > 0 ? message.left(questionEnd + 1) : title;
    const QString detail = questionEnd > 0 ? message.mid(questionEnd + 1).trimmed() : message;

    const bool deletes = message.contains(QLatin1String("delete"), Qt::CaseInsensitive)
                         || message.contains(QLatin1String("remove"), Qt::CaseInsensitive);
    MessageDialog dialog(deletes ? MessageDialog::Kind::Danger : MessageDialog::Kind::Question,
                         heading, detail, parent);
    dialog.setButtons(deletes ? QObject::tr("Delete") : QObject::tr("Yes"),
                      deletes ? QObject::tr("Cancel") : QObject::tr("No"));
    return dialog.exec() == QDialog::Accepted;
}

void showInfo(QWidget* parent, const QString& message, const QString& title)
{
    MessageDialog(MessageDialog::Kind::Success, title, message, parent).exec();
}

void showWarning(QWidget* parent, const QString& message, const QString& title)
{
    MessageDialog(MessageDialog::Kind::Warning, title, message, parent).exec();
}

void showError(QWidget* parent, const QString& message, const QString& title)
{
    MessageDialog(MessageDialog::Kind::Error, title, message, parent).exec();
}

QString generateTicketNumber()
{
    return QString::number(QDateTime::currentSecsSinceEpoch());
}

QString findSiblingPath(const QString& appName)
{
    QString appDir = QCoreApplication::applicationDirPath();

    // Depths relative to the running executable: same dir, parent, grandparent.
    // Covers sibling-subdir and shadow-build layouts.
    const QStringList depths = { "", "/..", "/../.." };
    // qmake places executables in debug/ or release/ on Windows shadow builds.
    const QStringList outDirs = { "", "/debug", "/release" };
    const QStringList names = { appName, appName + ".exe" };

    // Everything installed together in one folder.
    for (const QString& name : names) {
        const QString candidate = appDir + "/" + name;
        if (QFile::exists(candidate))
            return candidate;
    }

    for (const QString& depth : depths) {
        for (const QString& outDir : outDirs) {
            const QString dir = appDir + depth + "/" + appName + outDir;
            for (const QString& name : names) {
                const QString candidate = dir + "/" + name;
                if (QFile::exists(candidate))
                    return candidate;
            }
        }
    }
    return QString();
}

void bringToFront(QWidget* w)
{
    if (!w) return;
    if (w->isMinimized())
        w->setWindowState(w->windowState() & ~Qt::WindowMinimized);
    w->raise();
    w->activateWindow();
}

} // namespace AppHelpers
