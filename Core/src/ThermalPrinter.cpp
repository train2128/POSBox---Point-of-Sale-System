#include "ThermalPrinter.h"
#include "CommonConfig.h"
#include "PrinterConstants.h"

#include <QSqlQuery>
#include <QSqlRecord>
#include <QSerialPort>
#include <QStringEncoder>
#include <QImage>
#include <QPainter>
#include <QFile>
#include <QThread>

// ============================================================================
// CONSTRUCTOR
// ============================================================================

ThermalPrinter::ThermalPrinter(const QSqlDatabase &db)
    : m_db(db)
{
}

// ============================================================================
// PUBLIC — print()
// ============================================================================

bool ThermalPrinter::print(const TicketData &data)
{
    // --- Load printer settings from database ---
    QSqlQuery query(m_db);
    query.exec("SELECT port, baud_rate, data_bits, parity, stop_bits, flow_control, "
               "latency_ms, paper_size, encoding FROM printer_settings WHERE id = 1");
    if (!query.next()) {
        m_lastError = "Printer settings not found in database. Please configure the printer first.";
        return false;
    }

    const QString portName    = query.value("port").toString();
    const int    baudRate    = query.value("baud_rate").toInt();
    const int    dataBits    = query.value("data_bits").toInt();
    const QString parity      = query.value("parity").toString();
    const QString stopBits    = query.value("stop_bits").toString();
    const QString flowControl = query.value("flow_control").toString();
    const int    latencyMs   = query.value("latency_ms").toInt();
    m_encoding  = query.value("encoding").toString();
    m_paperSize = query.value("paper_size").toString();

    if (portName.isEmpty()) {
        m_lastError = "No port configured. Please select a COM port in printer settings.";
        return false;
    }

    // --- Configure serial port ---
    QSerialPort serial;
    serial.setPortName(portName);
    serial.setBaudRate(baudRate);

    // Data bits
    switch (dataBits) {
    case 5:  serial.setDataBits(QSerialPort::Data5); break;
    case 6:  serial.setDataBits(QSerialPort::Data6); break;
    case 7:  serial.setDataBits(QSerialPort::Data7); break;
    default: serial.setDataBits(QSerialPort::Data8); break;
    }

    // Parity
    if      (parity == "Odd")   serial.setParity(QSerialPort::OddParity);
    else if (parity == "Even")  serial.setParity(QSerialPort::EvenParity);
    else if (parity == "Mark")  serial.setParity(QSerialPort::MarkParity);
    else if (parity == "Space") serial.setParity(QSerialPort::SpaceParity);
    else                        serial.setParity(QSerialPort::NoParity);

    // Stop bits
    if      (stopBits == "1.5") serial.setStopBits(QSerialPort::OneAndHalfStop);
    else if (stopBits == "2")   serial.setStopBits(QSerialPort::TwoStop);
    else                        serial.setStopBits(QSerialPort::OneStop);

    // Flow control
    if      (flowControl == "Hardware") serial.setFlowControl(QSerialPort::HardwareControl);
    else if (flowControl == "Software") serial.setFlowControl(QSerialPort::SoftwareControl);
    else                                serial.setFlowControl(QSerialPort::NoFlowControl);

    if (!serial.open(QIODevice::WriteOnly)) {
        m_lastError = QString("Failed to open port %1: %2").arg(portName, serial.errorString());
        return false;
    }

    // --- Build and send ESC/POS data ---
    const QByteArray payload = buildEscPos(data);
    serial.write(payload);
    serial.waitForBytesWritten(3000);

    // Apply latency before closing
    if (latencyMs > 0)
        QThread::msleep(latencyMs);

    serial.close();
    return true;
}

// ============================================================================
// PRIVATE — buildEscPos()
// ============================================================================

QByteArray ThermalPrinter::buildEscPos(const TicketData &data) const
{
    const QString enc        = m_encoding.isEmpty() ? "UTF-8" : m_encoding;
    const QString paperSize  = m_paperSize.isEmpty() ? "80mm" : m_paperSize;
    const int     paperWidth = paperWidthChars(paperSize);

    QByteArray out;
    out += cmdInit();

    // ------------------------------------------------------------------
    // LOGO — GS v 0 raster mode (ESC/POS standard)
    // ------------------------------------------------------------------
    if (!data.shop.logoPath.isEmpty() && QFile::exists(data.shop.logoPath)) {
        QImage logo(data.shop.logoPath);
        if (!logo.isNull()) {
            // Scale to paper width in pixels (203 dpi ~8 dots per mm)
            const int maxPixelWidth = paperWidthPixels(paperSize);
            logo = logo.scaledToWidth(maxPixelWidth, Qt::SmoothTransformation);

            // Flatten to white background then convert to 1-bit monochrome
            QImage mono(logo.size(), QImage::Format_RGB32);
            mono.fill(Qt::white);
            QPainter painter(&mono);
            painter.drawImage(0, 0, logo);
            painter.end();
            mono = mono.convertToFormat(QImage::Format_Mono, Qt::ThresholdDither | Qt::MonoOnly);

            const int imgWidth  = mono.width();
            const int imgHeight = mono.height();
            const int byteWidth = (imgWidth + 7) / 8;

            // GS v 0: 0x1D 0x76 0x30 <mode> <xL> <xH> <yL> <yH>
            out += cmdAlignCenter();
            out += QByteArray("\x1D\x76\x30\x00", 4);
            out += static_cast<char>(byteWidth & 0xFF);
            out += static_cast<char>((byteWidth >> 8) & 0xFF);
            out += static_cast<char>(imgHeight & 0xFF);
            out += static_cast<char>((imgHeight >> 8) & 0xFF);

            // Raster data — row by row, MSB first, dark pixel = 1
            for (int y = 0; y < imgHeight; ++y) {
                for (int byteIdx = 0; byteIdx < byteWidth; ++byteIdx) {
                    unsigned char b = 0;
                    for (int bit = 0; bit < 8; ++bit) {
                        const int x = byteIdx * 8 + bit;
                        if (x < imgWidth) {
                            if (mono.pixelIndex(x, y) == 0)
                                b |= (1 << (7 - bit));
                        }
                    }
                    out += static_cast<char>(b);
                }
            }
            out += cmdFeedLines(1);
        }
    }

    // ------------------------------------------------------------------
    // SHOP NAME
    // ------------------------------------------------------------------
    out += cmdAlignCenter();
    out += cmdBoldOn();
    out += cmdDoubleHeightOn();
    out += cmdText(data.shop.name.isEmpty() ? "Shop Name" : data.shop.name, enc);
    out += QByteArray("\x0A"); // LF
    out += cmdDoubleHeightOff();
    out += cmdBoldOff();
    out += cmdFeedLines(1);

    // ------------------------------------------------------------------
    // UNPAID WARNING — for OPEN tickets
    // ------------------------------------------------------------------
    if (data.isUnpaid) {
        out += cmdAlignCenter();
        out += cmdBoldOn();
        out += cmdDoubleHeightOn();
        out += cmdText("================", enc);
        out += QByteArray("\x0A");
        out += cmdText("   UNPAID TICKET   ", enc);
        out += QByteArray("\x0A");
        out += cmdText(" NOT VALID UNTIL PAID ", enc);
        out += QByteArray("\x0A");
        out += cmdText("================", enc);
        out += QByteArray("\x0A");
        out += cmdDoubleHeightOff();
        out += cmdBoldOff();
        out += cmdFeedLines(2);
    }

    // ------------------------------------------------------------------
    // ITEMS HEADER LINE
    // ------------------------------------------------------------------
    out += cmdAlignLeft();
    out += cmdLine(paperWidth);

    // Items — name (left), qty (center), price (right)
    for (const TicketItem &item : data.items)
        out += formatItemRow(item, paperWidth, enc);

    // ------------------------------------------------------------------
    // SEPARATOR LINE
    // ------------------------------------------------------------------
    out += cmdLine(paperWidth);

    // ------------------------------------------------------------------
    // TOTALS
    // ------------------------------------------------------------------
    out += formatTotalRow("Subtotal",         data.subtotal,       paperWidth, enc);
    out += formatTotalRow("Discount",         data.discountAmount, paperWidth, enc);

    out += cmdBoldOn();
    out += formatTotalRow("Total",            data.total,          paperWidth, enc);
    out += cmdBoldOff();

    // ------------------------------------------------------------------
    // FOOTER LINE
    // ------------------------------------------------------------------
    out += cmdLine(paperWidth);
    out += cmdFeedLines(1);

    // ------------------------------------------------------------------
    // CONTACT INFO
    // ------------------------------------------------------------------
    out += cmdAlignCenter();
    if (!data.shop.phone.isEmpty())
        out += cmdText(data.shop.phone, enc) + QByteArray("\x0A");
    if (!data.shop.email.isEmpty())
        out += cmdText(data.shop.email, enc) + QByteArray("\x0A");

    // ------------------------------------------------------------------
    // FEED & CUT
    // ------------------------------------------------------------------
    out += cmdFeedLines(4);
    out += cmdCutPaper();

    return out;
}

// ============================================================================
// ESC/POS COMMAND BUILDERS
// ============================================================================

QByteArray ThermalPrinter::cmdInit() const
{
    return QByteArray("\x1B\x40", 2); // ESC @
}

QByteArray ThermalPrinter::cmdAlignLeft() const
{
    return QByteArray("\x1B\x61\x00", 3); // ESC a 0
}

QByteArray ThermalPrinter::cmdAlignCenter() const
{
    return QByteArray("\x1B\x61\x01", 3); // ESC a 1
}

QByteArray ThermalPrinter::cmdAlignRight() const
{
    return QByteArray("\x1B\x61\x02", 3); // ESC a 2
}

QByteArray ThermalPrinter::cmdBoldOn() const
{
    return QByteArray("\x1B\x45\x01", 3); // ESC E 1
}

QByteArray ThermalPrinter::cmdBoldOff() const
{
    return QByteArray("\x1B\x45\x00", 3); // ESC E 0
}

QByteArray ThermalPrinter::cmdDoubleHeightOn() const
{
    return QByteArray("\x1B\x21\x10", 3); // ESC ! 16
}

QByteArray ThermalPrinter::cmdDoubleHeightOff() const
{
    return QByteArray("\x1B\x21\x00", 3); // ESC ! 0
}

QByteArray ThermalPrinter::cmdFeedLines(int n) const
{
    QByteArray out;
    for (int i = 0; i < n; ++i)
        out += '\x0A';
    return out;
}

QByteArray ThermalPrinter::cmdCutPaper() const
{
    return QByteArray("\x1D\x56\x42\x00", 4); // GS V B 0 — partial cut
}

QByteArray ThermalPrinter::cmdText(const QString &text, const QString &encoding) const
{
    // Qt6 replacement for QTextCodec
    QStringEncoder encoder(encoding.toUtf8().constData());
    if (!encoder.isValid())
        encoder = QStringEncoder(QStringEncoder::Utf8);
    return encoder.encode(text);
}

QByteArray ThermalPrinter::cmdLine(int width) const
{
    return cmdText(QString(width, '-'), "UTF-8") + QByteArray("\x0A");
}

// ============================================================================
// LAYOUT HELPERS
// ============================================================================

QByteArray ThermalPrinter::formatItemRow(const TicketItem &item, int paperWidth, const QString &encoding) const
{
    // Layout: "Item Name          x2     10.00"
    // Name takes the left portion, qty in the middle, price right-aligned
    const QString qtyStr   = QString("x%1").arg(item.quantity);
    const QString priceStr = QString::number(item.quantity * item.unitPrice, 'f', 2);

    // Fixed column widths
    const int priceCol = 8;
    const int qtyCol   = 5;
    const int nameCol  = paperWidth - qtyCol - priceCol;

    QString name = item.name;
    if (name.length() > nameCol)
        name = name.left(nameCol - 1) + ".";

    const QString row = name.leftJustified(nameCol, ' ')
                        + qtyStr.rightJustified(qtyCol, ' ')
                        + priceStr.rightJustified(priceCol, ' ');

    return cmdText(row, encoding) + QByteArray("\x0A");
}

QByteArray ThermalPrinter::formatTotalRow(const QString &label, double amount, int paperWidth, const QString &encoding) const
{
    // Layout: "Label                   00.00"
    const QString amountStr = QString::number(amount, 'f', 2);
    const int amountCol     = 10;
    const int labelCol      = paperWidth - amountCol;

    const QString row = label.leftJustified(labelCol, ' ')
                        + amountStr.rightJustified(amountCol, ' ');

    return cmdText(row, encoding) + QByteArray("\x0A");
}

int ThermalPrinter::paperWidthChars(const QString &paperSize) const
{
    // Characters per line at default font size for each paper width
    if (paperSize == "58mm")   return 32;
    if (paperSize == "76mm")   return 42;
    if (paperSize == "80mm")   return 48;
    if (paperSize == "82.5mm") return 50;
    if (paperSize == "104mm")  return 64;
    if (paperSize == "112mm")  return 68;
    return 48; // fallback to 80mm
}

int ThermalPrinter::paperWidthPixels(const QString &paperSize) const
{
    // Printable pixel width at 203 dpi for each paper size
    // Formula: (paper_mm - margins_mm) * (203 / 25.4)
    if (paperSize == "58mm")   return 384;   // ~48mm printable
    if (paperSize == "76mm")   return 512;   // ~64mm printable
    if (paperSize == "80mm")   return 576;   // ~72mm printable
    if (paperSize == "82.5mm") return 608;   // ~76mm printable
    if (paperSize == "104mm")  return 816;   // ~102mm printable
    if (paperSize == "112mm")  return 880;   // ~110mm printable
    return 576; // fallback to 80mm
}
