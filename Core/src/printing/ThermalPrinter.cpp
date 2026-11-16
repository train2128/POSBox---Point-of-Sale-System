#include "ThermalPrinter.h"
#include "PrinterConstants.h"

#include <QElapsedTimer>
#include <QFont>
#include <QFontMetricsF>
#include <QPainter>
#include <QHash>
#include <QImage>
#include <QSerialPort>
#include <QStringEncoder>
#include <QThread>
#include <QtMath>

namespace {

// Unicode values of bytes 128-255. Qt can only convert these encodings when
// built with ICU, which isn't the case everywhere, so they're kept here.
const ushort Cp850Table[128] = {
    0x00C7, 0x00FC, 0x00E9, 0x00E2, 0x00E4, 0x00E0, 0x00E5, 0x00E7, 0x00EA, 0x00EB, 0x00E8, 0x00EF, 0x00EE, 0x00EC, 0x00C4, 0x00C5, 0x00C9, 0x00E6, 0x00C6, 0x00F4, 0x00F6, 0x00F2, 0x00FB, 0x00F9, 0x00FF, 0x00D6, 0x00DC, 0x00F8, 0x00A3, 0x00D8, 0x00D7, 0x0192, 0x00E1, 0x00ED, 0x00F3, 0x00FA, 0x00F1, 0x00D1, 0x00AA, 0x00BA, 0x00BF, 0x00AE, 0x00AC, 0x00BD, 0x00BC, 0x00A1, 0x00AB, 0x00BB, 0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x00C1, 0x00C2, 0x00C0, 0x00A9, 0x2563, 0x2551, 0x2557, 0x255D, 0x00A2, 0x00A5, 0x2510, 0x2514, 0x2534, 0x252C, 0x251C, 0x2500, 0x253C, 0x00E3, 0x00C3, 0x255A, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256C, 0x00A4, 0x00F0, 0x00D0, 0x00CA, 0x00CB, 0x00C8, 0x0131, 0x00CD, 0x00CE, 0x00CF, 0x2518, 0x250C, 0x2588, 0x2584, 0x00A6, 0x00CC, 0x2580, 0x00D3, 0x00DF, 0x00D4, 0x00D2, 0x00F5, 0x00D5, 0x00B5, 0x00FE, 0x00DE, 0x00DA, 0x00DB, 0x00D9, 0x00FD, 0x00DD, 0x00AF, 0x00B4, 0x00AD, 0x00B1, 0x2017, 0x00BE, 0x00B6, 0x00A7, 0x00F7, 0x00B8, 0x00B0, 0x00A8, 0x00B7, 0x00B9, 0x00B3, 0x00B2, 0x25A0, 0x00A0
};
const ushort Cp1252Table[128] = {
    0x20AC, 0xFFFD, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0xFFFD, 0x017D, 0xFFFD, 0xFFFD, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0xFFFD, 0x017E, 0x0178, 0x00A0, 0x00A1, 0x00A2, 0x00A3, 0x00A4, 0x00A5, 0x00A6, 0x00A7, 0x00A8, 0x00A9, 0x00AA, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x00AF, 0x00B0, 0x00B1, 0x00B2, 0x00B3, 0x00B4, 0x00B5, 0x00B6, 0x00B7, 0x00B8, 0x00B9, 0x00BA, 0x00BB, 0x00BC, 0x00BD, 0x00BE, 0x00BF, 0x00C0, 0x00C1, 0x00C2, 0x00C3, 0x00C4, 0x00C5, 0x00C6, 0x00C7, 0x00C8, 0x00C9, 0x00CA, 0x00CB, 0x00CC, 0x00CD, 0x00CE, 0x00CF, 0x00D0, 0x00D1, 0x00D2, 0x00D3, 0x00D4, 0x00D5, 0x00D6, 0x00D7, 0x00D8, 0x00D9, 0x00DA, 0x00DB, 0x00DC, 0x00DD, 0x00DE, 0x00DF, 0x00E0, 0x00E1, 0x00E2, 0x00E3, 0x00E4, 0x00E5, 0x00E6, 0x00E7, 0x00E8, 0x00E9, 0x00EA, 0x00EB, 0x00EC, 0x00ED, 0x00EE, 0x00EF, 0x00F0, 0x00F1, 0x00F2, 0x00F3, 0x00F4, 0x00F5, 0x00F6, 0x00F7, 0x00F8, 0x00F9, 0x00FA, 0x00FB, 0x00FC, 0x00FD, 0x00FE, 0x00FF
};

QHash<ushort, char> reverseTable(const ushort (&table)[128])
{
    QHash<ushort, char> map;
    for (int i = 0; i < 128; ++i)
        map.insert(table[i], char(128 + i));
    return map;
}

// Printed text in a single-byte code page. Characters it lacks get the closest
// plain spelling rather than a wrong symbol.
QByteArray encodeSingleByte(const QString &text, const QHash<ushort, char> &map)
{
    QByteArray out;
    out.reserve(text.size());
    for (const QChar c : text) {
        const ushort code = c.unicode();
        if (code < 128) {
            out += char(code);
        } else if (map.contains(code)) {
            out += map.value(code);
        } else if (code == 0x0153) {
            out += "oe";
        } else if (code == 0x0152) {
            out += "OE";
        } else if (code == 0x2018 || code == 0x2019) {
            out += '\'';
        } else if (code == 0x201C || code == 0x201D) {
            out += '"';
        } else if (code == 0x2013 || code == 0x2014) {
            out += '-';
        } else if (code == 0x2026) {
            out += "...";
        } else if (code == 0x00A0 || code == 0x202F) {
            out += ' ';
        } else {
            const QString plain = c.decomposition();
            const ushort base = plain.isEmpty() ? 0 : plain.at(0).unicode();
            out += (base > 0 && base < 128) ? char(base) : '?';
        }
    }
    return out;
}

QByteArray command(std::initializer_list<int> bytes)
{
    QByteArray out;
    for (int value : bytes)
        out += char(value);
    return out;
}

// GS v 0: a block of black and white dots, most significant bit first.
QByteArray rasterImage(const QImage &image)
{
    const int width = image.width();
    const int height = image.height();
    const int byteWidth = (width + 7) / 8;

    QByteArray out = command({0x1D, 0x76, 0x30, 0x00, byteWidth & 0xFF, (byteWidth >> 8) & 0xFF,
                              height & 0xFF, (height >> 8) & 0xFF});
    out.reserve(out.size() + byteWidth * height);
    for (int y = 0; y < height; ++y) {
        for (int index = 0; index < byteWidth; ++index) {
            unsigned char value = 0;
            for (int bit = 0; bit < 8; ++bit) {
                const int x = index * 8 + bit;
                if (x < width && qGray(image.pixel(x, y)) < 128)
                    value |= (1 << (7 - bit));
            }
            out += char(value);
        }
    }
    return out;
}

// True when every character prints in the single-byte code page, natively or
// through a plain spelling (é -> e). Other scripts (Arabic, Cyrillic, Chinese)
// are printed as pictures instead.
bool fitsCodePage(const QString &text, const QHash<ushort, char> &map)
{
    for (const QChar c : text) {
        const ushort code = c.unicode();
        if (code < 128 || map.contains(code))
            continue;
        switch (code) {
        case 0x0153: case 0x0152: case 0x2018: case 0x2019: case 0x201C: case 0x201D:
        case 0x2013: case 0x2014: case 0x2026: case 0x00A0: case 0x202F:
            continue;
        default:
            break;
        }
        const QString plain = c.decomposition();
        const ushort base = plain.isEmpty() ? 0 : plain.at(0).unicode();
        if (base == 0 || base >= 128)
            return false;
    }
    return true;
}

// One receipt line drawn by Qt, for scripts the printer's character set lacks.
// Same size and alignment as the printer's own font: `columns` characters fill
// `dots` pixels.
QImage lineImage(const Receipt::Line &line, int dots, int columns)
{
    const qreal cell = qreal(dots) / qMax(1, columns);
    QFont font;
    font.setFamilies({"DejaVu Sans Mono", "Consolas", "Liberation Mono", "Noto Sans Mono",
                      "Courier New", "Noto Sans", "Noto Sans Arabic", "Noto Sans CJK SC",
                      "Microsoft YaHei", "Tahoma"});
    font.setStyleHint(QFont::Monospace);
    font.setPixelSize(qMax(8, qRound(cell / 0.6)));
    font.setBold(line.bold);

    const QFontMetricsF metrics(font);
    const qreal scaleX = line.doubleWidth ? 2.0 : 1.0;
    const qreal scaleY = line.doubleHeight ? 2.0 : 1.0;
    const int height = qCeil(metrics.height() * scaleY) + 2;

    QImage image(dots, height, QImage::Format_RGB32);
    image.fill(line.reversed ? Qt::black : Qt::white);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::TextAntialiasing, false);
    painter.setFont(font);
    painter.setPen(line.reversed ? Qt::white : Qt::black);
    painter.scale(scaleX, scaleY);
    const QRectF area(0, 0, dots / scaleX, height / scaleY);
    const int align = line.align == 1 ? Qt::AlignHCenter : line.align == 2 ? Qt::AlignRight : Qt::AlignLeft;
    painter.drawText(area, align | Qt::AlignVCenter, line.text);
    painter.end();
    return image;
}

} // namespace

ThermalPrinter::ThermalPrinter(const QVariantMap &printerSettings)
    : m_printerCfg(printerSettings)
{
}

bool ThermalPrinter::print(const TicketData &data)
{
    if (m_printerCfg.value("enabled", 0).toInt() == 0) {
        m_lastError = "Printer is disabled. Enable it in Settings first.";
        return false;
    }

    const QString portName    = m_printerCfg.value("port").toString();
    const int     baudRate    = m_printerCfg.value("baud_rate", PrinterConstants::DEFAULT_BAUD_RATE).toInt();
    const int     dataBits    = m_printerCfg.value("data_bits", PrinterConstants::DEFAULT_DATA_BITS).toInt();
    const QString parity      = m_printerCfg.value("parity", "None").toString();
    const QString stopBits    = m_printerCfg.value("stop_bits", "1").toString();
    const QString flowControl = m_printerCfg.value("flow_control", "None").toString();
    const int     latencyMs   = m_printerCfg.value("latency_ms", PrinterConstants::DEFAULT_LATENCY).toInt();

    if (portName.isEmpty()) {
        m_lastError = "No port configured. Please select a COM port in printer settings.";
        return false;
    }

    QSerialPort serial;
    serial.setPortName(portName);
    serial.setBaudRate(baudRate);

    switch (dataBits) {
    case 5:  serial.setDataBits(QSerialPort::Data5); break;
    case 6:  serial.setDataBits(QSerialPort::Data6); break;
    case 7:  serial.setDataBits(QSerialPort::Data7); break;
    default: serial.setDataBits(QSerialPort::Data8); break;
    }

    if      (parity == "Odd")   serial.setParity(QSerialPort::OddParity);
    else if (parity == "Even")  serial.setParity(QSerialPort::EvenParity);
    else if (parity == "Mark")  serial.setParity(QSerialPort::MarkParity);
    else if (parity == "Space") serial.setParity(QSerialPort::SpaceParity);
    else                        serial.setParity(QSerialPort::NoParity);

    if      (stopBits == "1.5") serial.setStopBits(QSerialPort::OneAndHalfStop);
    else if (stopBits == "2")   serial.setStopBits(QSerialPort::TwoStop);
    else                        serial.setStopBits(QSerialPort::OneStop);

    if      (flowControl == "Hardware") serial.setFlowControl(QSerialPort::HardwareControl);
    else if (flowControl == "Software") serial.setFlowControl(QSerialPort::SoftwareControl);
    else                                serial.setFlowControl(QSerialPort::NoFlowControl);

    if (!serial.open(QIODevice::WriteOnly)) {
        m_lastError = QString("Failed to open port %1: %2").arg(portName, serial.errorString());
        return false;
    }

    const QByteArray payload = buildEscPos(data);
    if (serial.write(payload) != payload.size()) {
        m_lastError = QString("Could not send the ticket to %1: %2").arg(portName, serial.errorString());
        serial.close();
        return false;
    }

    // A ticket with a logo can take several seconds at low speeds, so the time
    // allowed follows its size: about 10 bits per byte, doubled, plus a margin.
    const qint64 allowedMs = payload.size() * 20000LL / qMax(1, baudRate) + 3000;
    QElapsedTimer timer;
    timer.start();
    while (serial.bytesToWrite() > 0) {
        const qint64 left = allowedMs - timer.elapsed();
        if (left <= 0 || (!serial.waitForBytesWritten(int(left)) && serial.bytesToWrite() > 0)) {
            m_lastError = QString("The printer on %1 did not take the whole ticket. Check that it is on and has paper.")
                              .arg(portName);
            serial.close();
            return false;
        }
    }

    if (latencyMs > 0)
        QThread::msleep(latencyMs);

    serial.close();
    m_lastError.clear();
    return true;
}

QByteArray ThermalPrinter::encodeText(const QString &text) const
{
    const QString encoding = m_printerCfg.value("encoding", PrinterConstants::DEFAULT_ENCODING).toString();
    if (encoding == QLatin1String("IBM850")) {
        static const QHash<ushort, char> map = reverseTable(Cp850Table);
        return encodeSingleByte(text, map);
    }
    if (encoding == QLatin1String("CP1252")) {
        static const QHash<ushort, char> map = reverseTable(Cp1252Table);
        return encodeSingleByte(text, map);
    }
    if (encoding == QLatin1String("IMAGE"))
        return text.toUtf8();  // not used: every line is printed as a picture
    QStringEncoder encoder(encoding.toUtf8().constData());
    if (!encoder.isValid())
        encoder = QStringEncoder(QStringEncoder::Utf8);
    return encoder.encode(text);
}

QByteArray ThermalPrinter::buildEscPos(const TicketData &data) const
{
    const QString paperSize = m_printerCfg.value("paper_size", PrinterConstants::DEFAULT_PAPER_SIZE).toString();
    const QString encoding = m_printerCfg.value("encoding", PrinterConstants::DEFAULT_ENCODING).toString();

    QByteArray out = command({0x1B, 0x40});  // ESC @: start clean
    if (encoding == QLatin1String("IBM850"))
        out += command({0x1B, 0x74, 2});     // ESC t: code page
    else if (encoding == QLatin1String("CP1252"))
        out += command({0x1B, 0x74, 16});

    // Text the printer's character set can't show goes out as a picture.
    const bool alwaysImage = encoding == QLatin1String("IMAGE");
    const QHash<ushort, char>* singleByte = nullptr;
    static const QHash<ushort, char> cp850 = reverseTable(Cp850Table);
    static const QHash<ushort, char> cp1252 = reverseTable(Cp1252Table);
    if (encoding == QLatin1String("IBM850"))
        singleByte = &cp850;
    else if (encoding == QLatin1String("CP1252"))
        singleByte = &cp1252;
    const int dots = Receipt::dotsFor(paperSize);

    const QList<Receipt::Line> lines = Receipt::layout(data, paperSize);
    for (const Receipt::Line &line : lines) {
        out += command({0x1B, 0x61, line.align});  // ESC a: alignment
        if (line.isLogo) {
            const QImage bitmap = Receipt::logoBitmap(data.shop.logoImage, line.logoDots);
            if (!bitmap.isNull()) {
                out += rasterImage(bitmap);
                out += '\n';
            }
            continue;
        }
        if (alwaysImage || (singleByte && !fitsCodePage(line.text, *singleByte))) {
            const int columns = line.small ? Receipt::smallColumnsFor(paperSize)
                                           : Receipt::columnsFor(paperSize);
            out += command({0x1B, 0x61, 0});
            out += rasterImage(lineImage(line, dots, columns));
            continue;
        }
        out += command({0x1B, 0x4D, line.small ? 1 : 0});                // ESC M: font
        out += command({0x1D, 0x21, (line.doubleWidth ? 0x10 : 0) | (line.doubleHeight ? 0x01 : 0)});  // GS !: size
        out += command({0x1B, 0x45, line.bold ? 1 : 0});                 // ESC E: bold
        out += command({0x1D, 0x42, line.reversed ? 1 : 0});             // GS B: white on black
        out += encodeText(line.text);
        out += '\n';
    }

    // Back to normal, feed past the cutter and cut.
    out += command({0x1D, 0x42, 0, 0x1B, 0x45, 0, 0x1D, 0x21, 0, 0x1B, 0x4D, 0, 0x1B, 0x61, 0});
    out += QByteArray(4, '\n');
    out += command({0x1D, 0x56, 0x42, 0});  // GS V: partial cut
    return out;
}
