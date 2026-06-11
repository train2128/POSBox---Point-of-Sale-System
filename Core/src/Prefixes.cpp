#include "Prefixes.h"
#include <QMessageBox>
#include <QDateTime>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QDebug>

namespace AppHelpers {

void loadCurrencyConfig(QSqlDatabase db)
{
    if (!db.isValid() || !db.isOpen()) return;

    QSqlQuery query(db);
    query.prepare("SELECT currency FROM app_settings WHERE id = 1");
    if (query.exec() && query.next()) {
        QString cur = query.value("currency").toString().trimmed();
        if (!cur.isEmpty()) {
            AppConstants::CURRENCY = qstrdup(cur.toUtf8().constData());
        }
    }

    // Derive decimals from currency code
    // Standard: 0 for JPY/KRW/VND/UGX/IDR/RWF/XAF/XOF/GNF/CLP/COP/PYG/MGA/MMK/TWD/IRR/IQD/VES/DZD/ISK
    //           3 for TND/BHD/KWD/OMR/JOD LYD
    //           2 for everything else
    static const QSet<QString> zeroDecimals = {
        "JPY", "KRW", "VND", "UGX", "IDR", "RWF", "XAF", "XOF", "GNF",
        "CLP", "COP", "PYG", "MGA", "MMK", "TWD", "IRR", "IQD", "VES",
        "DZD", "ISK", "DJF", "BIF", "KHR", "LAK", "MRO"
    };
    static const QSet<QString> threeDecimals = {
        "TND", "BHD", "KWD", "OMR", "JOD", "LYD", "IQD"
    };

    QString curUpper = QString(AppConstants::CURRENCY).toUpper();
    if (threeDecimals.contains(curUpper))
        AppConstants::PRICE_DECIMALS = 3;
    else if (zeroDecimals.contains(curUpper))
        AppConstants::PRICE_DECIMALS = 0;
    else
        AppConstants::PRICE_DECIMALS = 2;

    qDebug() << "Currency loaded:" << AppConstants::CURRENCY
             << "Decimals:" << AppConstants::PRICE_DECIMALS;
}

QString formatPrice(double price)
{
    return QString("%1 %2")
    .arg(price, 0, 'f', AppConstants::PRICE_DECIMALS)
        .arg(AppConstants::CURRENCY);
}

QString formatDate(const QDate& date)
{
    return date.toString(AppConstants::DATETIME_DISPLAY).split(' ')[0];
}

QString formatDateTime(const QDateTime& dt)
{
    return dt.toString(AppConstants::DATETIME_DISPLAY);
}

QString formatDateTimeFile(const QDateTime& dt)
{
    return dt.toString(AppConstants::DATETIME_FILE);
}

bool confirmAction(QWidget* parent, const QString& message, const QString& title)
{
    return QMessageBox::question(parent, title, message,
                                 QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes;
}

void showInfo(QWidget* parent, const QString& message, const QString& title)
{
    QMessageBox::information(parent, title, message);
}

void showWarning(QWidget* parent, const QString& message, const QString& title)
{
    QMessageBox::warning(parent, title, message);
}

void showError(QWidget* parent, const QString& message, const QString& title)
{
    QMessageBox::critical(parent, title, message);
}

QString generateTicketNumber()
{
    return QString::number(QDateTime::currentSecsSinceEpoch());
}

} // namespace AppHelpers
