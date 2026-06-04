#ifndef THERMALPRINTER_H
#define THERMALPRINTER_H

#include <QString>
#include <QList>
#include <QSqlDatabase>

/**
 * ThermalPrinter
 *
 * Handles all ESC/POS ticket printing for POSBox.
 * Reads printer_settings from database for port configuration.
 * Accepts a TicketData struct and renders the full ticket layout.
 *
 * Used by: Settings (test print), Tickets app (real tickets).
 */
class ThermalPrinter
{
public:
    // -------------------------------------------------------------------------
    // Data structures passed by the caller
    // -------------------------------------------------------------------------

    struct ShopInfo {
        QString name;
        QString phone;
        QString email;
        QString logoPath;   // Absolute path to logo image, empty = skip
    };

    struct TicketItem {
        QString name;
        int     quantity;
        double  unitPrice;
    };

    struct TicketData {
        ShopInfo            shop;
        QList<TicketItem>   items;
        double              subtotal        = 0.0;
        double              discountAmount  = 0.0;
        double              total           = 0.0;
        bool                isUnpaid        = false;  // true for OPEN tickets
    };

    // -------------------------------------------------------------------------
    // Public API
    // -------------------------------------------------------------------------

    ThermalPrinter() = default;
    explicit ThermalPrinter(const QSqlDatabase &db);

    /**
     * Reads printer_settings from database, opens the serial port,
     * renders and sends the full ticket ESC/POS layout, then closes the port.
     *
     * @param data  The ticket content to print.
     * @return true on success, false on any failure.
     */
    bool print(const TicketData &data);

    /**
     * Returns the last error message if print() returned false.
     */
    QString lastError() const { return m_lastError; }

private:
    QString m_lastError;
    QSqlDatabase m_db;
    QString m_encoding;
    QString m_paperSize;

    QByteArray buildEscPos(const TicketData &data) const;

    // ESC/POS helper builders
    QByteArray cmdInit()            const;
    QByteArray cmdAlignLeft()       const;
    QByteArray cmdAlignCenter()     const;
    QByteArray cmdAlignRight()      const;
    QByteArray cmdBoldOn()          const;
    QByteArray cmdBoldOff()         const;
    QByteArray cmdDoubleHeightOn()  const;
    QByteArray cmdDoubleHeightOff() const;
    QByteArray cmdFeedLines(int n)  const;
    QByteArray cmdCutPaper()        const;
    QByteArray cmdText(const QString &text, const QString &encoding) const;
    QByteArray cmdLine(int width)   const;

    // Layout helpers
    QByteArray formatItemRow(const TicketItem &item, int paperWidth, const QString &encoding) const;
    QByteArray formatTotalRow(const QString &label, double amount, int paperWidth, const QString &encoding) const;
    int paperWidthChars(const QString &paperSize) const;
    int paperWidthPixels(const QString &paperSize) const;
};

#endif // THERMALPRINTER_H
