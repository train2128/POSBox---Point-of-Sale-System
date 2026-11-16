#ifndef THERMALPRINTER_H
#define THERMALPRINTER_H

#include "ReceiptDesign.h"
#include <QByteArray>
#include <QString>
#include <QVariantMap>

/**
 * ThermalPrinter
 *
 * Turns a ticket into ESC/POS commands, following its receipt design, and sends
 * them to the serial port set in the local settings.
 *
 * Used by: Settings (test print), Register and Tickets (real tickets).
 */
class ThermalPrinter
{
public:
    using ShopInfo = Receipt::ShopInfo;
    using TicketItem = Receipt::TicketItem;
    using TicketData = Receipt::TicketData;

    ThermalPrinter() = default;
    explicit ThermalPrinter(const QVariantMap &printerSettings);

    /**
     * Opens the configured serial port, sends the whole ticket, then closes it.
     * Returns false, with lastError() set, if the ticket didn't all go out.
     */
    bool print(const TicketData &data);

    QString lastError() const { return m_lastError; }

    // The bytes sent to the printer. Public so they can be checked without one.
    QByteArray buildEscPos(const TicketData &data) const;

private:
    QByteArray encodeText(const QString &text) const;

    QString m_lastError;
    QVariantMap m_printerCfg;
};

#endif // THERMALPRINTER_H
