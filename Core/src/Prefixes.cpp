#include "Prefixes.h"
#include <QMessageBox>
#include <QDateTime>

namespace AppHelpers {

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
