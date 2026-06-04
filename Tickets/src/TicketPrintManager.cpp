#include "TicketPrintManager.h"
#include "DatabaseManager.h"
#include "QueryRepository.h"
#include "CommonConfig.h"
#include <QTextDocument>
#include <QPrintPreviewDialog>
#include <QPrinter>
#include <QPageSize>
#include <QPageLayout>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlError>
#include <QSqlDatabase>
#include <QTextCursor>
#include <QTextTable>
#include <QTextTableFormat>
#include <QTextTableCell>
#include <QTextCharFormat>
#include <QTextBlockFormat>
#include <QTextFrame>
#include <QTextFrameFormat>
#include <QFont>
#include <QColor>
#include <QBrush>
#include <QTextLength>
#include <QPixmap>
#include <QFile>
#include <QDate>
#include <QDebug>
#include <QDir>
#include <QStandardPaths>

TicketPrintManager::TicketPrintManager(DatabaseManager* db, QWidget* parent)
    : QObject(parent)
    , m_db(db)
    , m_parent(parent)
    , m_currentDocument(nullptr)
    , m_currentTicketId(0)
{
}

void TicketPrintManager::printTicketWithPreview(int ticketId, const CustomerInvoiceData& customerData)
{
    if (!m_db || !m_db->isOpen()) {
        qWarning() << "TicketPrintManager: database not available";
        return;
    }

    m_currentTicketId = ticketId;
    m_currentCustomerData = customerData;

    m_currentDocument = new QTextDocument(this);
    m_currentDocument->setDefaultFont(QFont("Arial", 10));
    m_currentDocument->setDefaultStyleSheet("* { color: #000000; }");
    generateInvoiceDocument(m_currentDocument, ticketId, customerData);

    if (m_currentDocument->isEmpty()) {
        qWarning() << "TicketPrintManager: failed to generate invoice document";
        delete m_currentDocument;
        m_currentDocument = nullptr;
        return;
    }

    // Save PDF to Invoices folder
    savePdfToInvoices(getTicketRecord(ticketId).value("ticket_number").toString(), m_currentDocument);

    QPrinter printer(QPrinter::HighResolution);
    printer.setPageSize(QPageSize::A4);
    printer.setPageOrientation(QPageLayout::Portrait);
    printer.setPageMargins(QMarginsF(10, 10, 10, 10), QPageLayout::Millimeter);
    printer.setFromTo(0, 0);

    QPrintPreviewDialog preview(&printer, m_parent);
    preview.setWindowTitle("Print Preview - Invoice");
    preview.resize(900, 700);

    connect(&preview, &QPrintPreviewDialog::paintRequested,
            this, &TicketPrintManager::printPreview);

    preview.exec();

    delete m_currentDocument;
    m_currentDocument = nullptr;
}

void TicketPrintManager::printPreview(QPrinter* printer)
{
    if (m_currentDocument)
        m_currentDocument->print(printer);
}

void TicketPrintManager::generateInvoiceDocument(QTextDocument* document, int ticketId, const CustomerInvoiceData& customerData)
{
    QSqlRecord ticketRecord = getTicketRecord(ticketId);
    if (ticketRecord.isEmpty()) {
        qWarning() << "TicketPrintManager: ticket not found:" << ticketId;
        return;
    }

    QList<QSqlRecord> itemRecords = getTicketItemsRecord(ticketId);

    document->setDefaultFont(QFont("Arial", 10));
    document->setDefaultStyleSheet("body { font-family: Arial; }");

    QTextCursor cursor(document);

    addCompanyHeader(cursor);
    addInvoiceTitle(cursor,
                    ticketRecord.value("ticket_number").toString(),
                    ticketRecord.value("date").toString());
    addCompanyInfo(cursor);

    if (!customerData.isEmpty()) {
        addCustomerDetails(cursor, customerData);
    }

    double subtotal       = ticketRecord.value("subtotal").toDouble();
    double discountAmount = ticketRecord.value("discount_amount").toDouble();
    double total          = ticketRecord.value("total").toDouble();

    addItemsTable(cursor, itemRecords, subtotal, discountAmount, total);
    addSummary(cursor, total);
    addStampAndSignature(cursor);

    cursor.movePosition(QTextCursor::End);
    cursor.insertBlock();

    addFooter(cursor);
}

// ── Header (logo) ─────────────────────────────────────────────────────────────

void TicketPrintManager::addCompanyHeader(QTextCursor& cursor)
{
    QSqlRecord shopProfile = getShopProfileRecord();

    // logo_path stores only the relative filename — reconstruct the full path at runtime
    QString logoFilename = shopProfile.value("logo_path").toString();
    QString logoPath;
    if (!logoFilename.isEmpty()) {
        logoPath = POSConfig::getAppDataPath()
        + POSConfig::LOGO_IMAGE_FOLDER
            + "/" + logoFilename;
    }

    QTextTableFormat tableFormat;
    tableFormat.setBorder(0);
    tableFormat.setCellPadding(0);
    tableFormat.setCellSpacing(0);
    tableFormat.setWidth(QTextLength(QTextLength::PercentageLength, 100));

    QTextTable* logoTable = cursor.insertTable(1, 1, tableFormat);

    if (!logoPath.isEmpty() && QFile::exists(logoPath)) {
        QPixmap logo(logoPath);
        if (!logo.isNull()) {
            logo = logo.scaledToHeight(100, Qt::SmoothTransformation);

            QTextBlockFormat format;
            format.setAlignment(Qt::AlignRight);
            QTextCursor cellCursor = logoTable->cellAt(0, 0).firstCursorPosition();
            cellCursor.setBlockFormat(format);
            cellCursor.insertImage(logo.toImage(), logoPath);
        }
    }

    cursor.movePosition(QTextCursor::End);
    cursor.insertBlock();
    cursor.insertBlock();
}

// ── Invoice title and date ────────────────────────────────────────────────────

void TicketPrintManager::addInvoiceTitle(QTextCursor& cursor,
                                         const QString& ticketNumber,
                                         const QString& date)
{
    cursor.insertBlock(getLeftFormat());
    cursor.insertText("Facture No. " + ticketNumber, getTitleFormat());

    cursor.insertBlock();

    QTextBlockFormat rightAlign;
    rightAlign.setAlignment(Qt::AlignRight);
    cursor.insertBlock(rightAlign);

    // Use the ticket's own date, fall back to today
    QDate ticketDate = QDate::fromString(date, "yyyy-MM-dd");
    QString dateStr = ticketDate.isValid()
                          ? ticketDate.toString("dd/MM/yyyy")
                          : QDate::currentDate().toString("dd/MM/yyyy");

    cursor.insertText("Le " + dateStr, getBoldFormat());
    cursor.insertBlock();
}

// ── Company info (from shop_profile) ─────────────────────────────────────────

void TicketPrintManager::addCompanyInfo(QTextCursor& cursor)
{
    QSqlRecord shopProfile = getShopProfileRecord();
    if (shopProfile.isEmpty()) return;

    QString shopName = shopProfile.value("shop_name").toString();
    QString taxId    = shopProfile.value("tax_id").toString();
    QString address  = shopProfile.value("address").toString();
    QString postal   = shopProfile.value("postal").toString();
    QString city     = shopProfile.value("city").toString();

    cursor.insertBlock(getLeftFormat());
    cursor.insertText(shopName, getCompanyNameFormat());
    cursor.insertBlock(getLeftFormat());
    cursor.insertBlock();

    if (!taxId.isEmpty()) {
        cursor.insertText("M.F: " + taxId, getNormalFormat());
        cursor.insertBlock(getLeftFormat());
    }

    if (!address.isEmpty()) {
        cursor.insertText(address, getNormalFormat());
        cursor.insertBlock(getLeftFormat());
    }

    QString location;
    if (!postal.isEmpty() && !city.isEmpty())
        location = postal + " " + city;
    else if (!postal.isEmpty())
        location = postal;
    else if (!city.isEmpty())
        location = city;

    if (!location.isEmpty()) {
        cursor.insertText(location, getNormalFormat());
        cursor.insertBlock(getLeftFormat());
    }

    cursor.insertBlock();
}

// ── Customer details (for company invoices) ─────────────────────────────────

void TicketPrintManager::addCustomerDetails(QTextCursor& cursor, const CustomerInvoiceData& customerData)
{
    if (customerData.isEmpty()) return;

    cursor.insertBlock(getLeftFormat());
    cursor.insertBlock(getLeftFormat());

    // Client name on same line as "Client:"
    cursor.insertText("Client: " + customerData.companyName, getBoldFormat());
    cursor.insertBlock(getLeftFormat());

    // Matricule Fiscal
    if (!customerData.matriculeFiscal.isEmpty()) {
        cursor.insertText("M.F: " + customerData.matriculeFiscal, getNormalFormat());
        cursor.insertBlock(getLeftFormat());
    }

    // Address
    if (!customerData.address.isEmpty()) {
        cursor.insertText(customerData.address, getNormalFormat());
        cursor.insertBlock(getLeftFormat());
    }

    // Phone
    if (!customerData.phone.isEmpty()) {
        cursor.insertText("Phone: " + customerData.phone, getNormalFormat());
        cursor.insertBlock(getLeftFormat());
    }

    cursor.insertBlock();
    cursor.insertBlock(getLeftFormat());
}

// ── PDF Save ───────────────────────────────────────────────────────────────

QString TicketPrintManager::documentsRootPath() const
{
    const QString root = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                         + "/POSBox/";
    return root;
}

void TicketPrintManager::savePdfToInvoices(const QString& ticketNumber, QTextDocument* document)
{
    if (ticketNumber.isEmpty() || !document) return;

    QString root = documentsRootPath();
    QString invoicesPath = root + "Invoices/";

    QDir().mkpath(invoicesPath);

    QString filePath = invoicesPath + ticketNumber + ".pdf";

    QPrinter printer(QPrinter::HighResolution);
    printer.setPageSize(QPageSize::A4);
    printer.setPageOrientation(QPageLayout::Portrait);
    printer.setPageMargins(QMarginsF(10, 10, 10, 10), QPageLayout::Millimeter);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(filePath);

    document->print(&printer);

    qDebug() << "TicketPrintManager: PDF saved to" << filePath;
}

// ── Items table (product / qty / unit price / total) ─────────────────────────

void TicketPrintManager::addItemsTable(QTextCursor& cursor,
                                       const QList<QSqlRecord>& items,
                                       double subtotal,
                                       double discountAmount,
                                       double total)
{
    if (items.isEmpty()) {
        cursor.insertText("No items found.", getNormalFormat());
        return;
    }

    QTextTableFormat tableFormat;
    tableFormat.setBorder(0);
    tableFormat.setBorderStyle(QTextFrameFormat::BorderStyle_None);
    tableFormat.setCellPadding(8);
    tableFormat.setCellSpacing(0);
    tableFormat.setWidth(QTextLength(QTextLength::PercentageLength, 96));
    tableFormat.setBackground(QBrush(Qt::white));

    QVector<QTextLength> constraints;
    constraints << QTextLength(QTextLength::PercentageLength, 45);  // Product
    constraints << QTextLength(QTextLength::PercentageLength, 15);  // Qty
    constraints << QTextLength(QTextLength::PercentageLength, 20);  // Unit price
    constraints << QTextLength(QTextLength::PercentageLength, 20);  // Total
    tableFormat.setColumnWidthConstraints(constraints);

    // rows = header + items + subtotal + discount + total
    QTextTable* table = cursor.insertTable(items.size() + 4, 4, tableFormat);

    QColor headerBg("#5A5A5A");
    QColor oddRowBg("#F0F0F0");
    QColor evenRowBg("#FFFFFF");
    QColor totalRowBg("#D3D3D3");

    QTextBlockFormat centerAlign;
    centerAlign.setAlignment(Qt::AlignCenter);
    QTextBlockFormat leftAlign;
    leftAlign.setAlignment(Qt::AlignLeft);

    // ── Header row ────────────────────────────────────────────────────────────
    const QStringList headers = { "Produit", "Quantité", "Prix Unitaire", "Total" };
    for (int col = 0; col < 4; ++col) {
        QTextTableCell cell = table->cellAt(0, col);
        QTextTableCellFormat cellFmt;
        cellFmt.setBackground(QBrush(headerBg));
        cell.setFormat(cellFmt);

        QTextCursor cc = cell.firstCursorPosition();
        cc.setBlockFormat(col == 0 ? leftAlign : centerAlign);

        QTextCharFormat hdrFmt = getBoldFormat();
        hdrFmt.setForeground(QBrush(Qt::white));
        cc.setCharFormat(hdrFmt);
        cc.insertText(headers[col]);
    }

    // ── Data rows ─────────────────────────────────────────────────────────────
    for (int row = 1; row <= items.size(); ++row) {
        const QSqlRecord& item = items[row - 1];
        QColor rowBg = (row % 2) ? oddRowBg : evenRowBg;

        for (int col = 0; col < 4; ++col) {
            QTextTableCell cell = table->cellAt(row, col);
            QTextTableCellFormat cellFmt;
            cellFmt.setBackground(QBrush(rowBg));
            cell.setFormat(cellFmt);

            QTextCursor cc = cell.firstCursorPosition();
            cc.setBlockFormat(col == 0 ? leftAlign : centerAlign);
            cc.setCharFormat(getNormalFormat());

            QString text;
            switch (col) {
            case 0: text = item.value("product_name").toString(); break;
            case 1: text = QString::number(item.value("quantity").toInt()); break;
            case 2: text = formatCurrency(item.value("unit_price").toDouble()); break;
            case 3: text = formatCurrency(item.value("total_price").toDouble()); break;
            }
            cc.insertText(text);
        }
    }

    // ── Subtotal row ──────────────────────────────────────────────────────────
    int subtotalRow = items.size() + 1;
    table->mergeCells(subtotalRow, 0, 1, 3);
    for (int col = 0; col < 4; ++col) {
        QTextTableCellFormat f; f.setBackground(QBrush(totalRowBg));
        table->cellAt(subtotalRow, col).setFormat(f);
    }
    {
        QTextCursor cc = table->cellAt(subtotalRow, 0).firstCursorPosition();
        cc.setBlockFormat(centerAlign); cc.setCharFormat(getBoldFormat());
        cc.insertText("SOUS-TOTAL");
    }
    {
        QTextCursor cc = table->cellAt(subtotalRow, 3).firstCursorPosition();
        cc.setBlockFormat(centerAlign); cc.setCharFormat(getBoldFormat());
        cc.insertText(formatCurrency(subtotal));
    }

    // ── Discount row ──────────────────────────────────────────────────────────
    int discountRow = items.size() + 2;
    table->mergeCells(discountRow, 0, 1, 3);
    for (int col = 0; col < 4; ++col) {
        QTextTableCellFormat f; f.setBackground(QBrush(totalRowBg));
        table->cellAt(discountRow, col).setFormat(f);
    }
    {
        QTextCursor cc = table->cellAt(discountRow, 0).firstCursorPosition();
        cc.setBlockFormat(centerAlign); cc.setCharFormat(getBoldFormat());
        cc.insertText("REMISE");
    }
    {
        QTextCursor cc = table->cellAt(discountRow, 3).firstCursorPosition();
        cc.setBlockFormat(centerAlign); cc.setCharFormat(getBoldFormat());
        cc.insertText("-" + formatCurrency(discountAmount));
    }

    // ── Total row ─────────────────────────────────────────────────────────────
    int totalRow = items.size() + 3;
    table->mergeCells(totalRow, 0, 1, 3);
    for (int col = 0; col < 4; ++col) {
        QTextTableCellFormat f; f.setBackground(QBrush(totalRowBg));
        table->cellAt(totalRow, col).setFormat(f);
    }
    {
        QTextCursor cc = table->cellAt(totalRow, 0).firstCursorPosition();
        cc.setBlockFormat(centerAlign); cc.setCharFormat(getBoldFormat());
        cc.insertText("TOTAL");
    }
    {
        QTextCursor cc = table->cellAt(totalRow, 3).firstCursorPosition();
        cc.setBlockFormat(centerAlign); cc.setCharFormat(getBoldFormat());
        cc.insertText(formatCurrency(total));
    }

    cursor.movePosition(QTextCursor::End);
    cursor.insertBlock();
    cursor.insertBlock();
}

// ── Total in French words ─────────────────────────────────────────────────────

void TicketPrintManager::addSummary(QTextCursor& cursor, double total)
{
    cursor.insertBlock(getLeftFormat());
    cursor.insertText("Arrêté la présente facture à la somme de : ", getBoldFormat());
    cursor.insertText(numberToFrenchWords(total), getBoldFormat());
    cursor.insertBlock();
    cursor.insertBlock();
}

// ── Stamp and signature box ───────────────────────────────────────────────────

void TicketPrintManager::addStampAndSignature(QTextCursor& cursor)
{
    QSqlRecord shopProfile = getShopProfileRecord();
    QString city = shopProfile.value("city").toString();
    if (city.isEmpty()) city = "Le";

    QString dateStr = QDate::currentDate().toString("dd/MM/yyyy");

    QTextTableFormat tableFormat;
    tableFormat.setBorder(0);
    tableFormat.setCellPadding(0);
    tableFormat.setCellSpacing(0);
    tableFormat.setWidth(QTextLength(QTextLength::PercentageLength, 100));

    QVector<QTextLength> constraints;
    constraints << QTextLength(QTextLength::PercentageLength, 60);
    constraints << QTextLength(QTextLength::PercentageLength, 40);
    tableFormat.setColumnWidthConstraints(constraints);

    QTextTable* stampTable = cursor.insertTable(1, 2, tableFormat);
    QTextCursor stampCursor = stampTable->cellAt(0, 1).firstCursorPosition();

    QTextFrameFormat stampFrameFormat;
    stampFrameFormat.setBorder(2);
    stampFrameFormat.setBorderStyle(QTextFrameFormat::BorderStyle_Solid);
    stampFrameFormat.setBorderBrush(QBrush(Qt::black));
    stampFrameFormat.setBackground(QBrush(Qt::white));
    stampFrameFormat.setPadding(8);
    stampFrameFormat.setMargin(2);

    QTextFrame* stampFrame = stampCursor.insertFrame(stampFrameFormat);
    QTextCursor frameCursor = stampFrame->firstCursorPosition();

    QTextBlockFormat centerFormat;
    centerFormat.setAlignment(Qt::AlignCenter);

    frameCursor.insertBlock(centerFormat);
    frameCursor.insertText("Cachet et signature", getBoldFormat());

    for (int i = 0; i < 3; ++i) {
        frameCursor.insertBlock(centerFormat);
        frameCursor.insertText("                ");
    }

    frameCursor.insertBlock(centerFormat);
    frameCursor.insertText(city + " le " + dateStr, getNormalFormat());

    cursor.movePosition(QTextCursor::End);
    cursor.insertBlock();
}

// ── Footer (phone, email, social) ────────────────────────────────────────────

void TicketPrintManager::addFooter(QTextCursor& cursor)
{
    QSqlRecord shopProfile = getShopProfileRecord();
    if (shopProfile.isEmpty()) return;

    QList<QPair<QString, QString>> footerItems;

    QString phone = shopProfile.value("phone").toString();
    QString email = shopProfile.value("email").toString();

    if (!phone.isEmpty())
        footerItems.append({ "Tél:", phone });
    if (!email.isEmpty())
        footerItems.append({ "Email:", email });

    auto hasField = [&](const QString& field) {
        QString val = shopProfile.value(field).toString();
        return !val.isEmpty();
    };

    if (hasField("facebook"))
        footerItems.append({ "Facebook:", shopProfile.value("facebook").toString() });
    if (hasField("instagram"))
        footerItems.append({ "Instagram:", shopProfile.value("instagram").toString() });
    if (hasField("tiktok"))
        footerItems.append({ "TikTok:", shopProfile.value("tiktok").toString() });
    if (hasField("whatsapp"))
        footerItems.append({ "WhatsApp:", shopProfile.value("whatsapp").toString() });

    if (footerItems.isEmpty()) return;

    cursor.insertBlock(getLeftFormat());
    cursor.insertText(QString(60, '_'), getNormalFormat());
    cursor.insertBlock();

    QTextCharFormat labelFmt = getNormalFormat();
    labelFmt.setFont(QFont("Arial", 9, QFont::Bold));
    labelFmt.setForeground(QBrush(QColor("#2F5496")));

    QTextCharFormat textFmt = getNormalFormat();
    textFmt.setFont(QFont("Arial", 9));

    for (const auto& item : footerItems) {
        cursor.insertBlock(getLeftFormat());
        cursor.insertText(item.first + " ", labelFmt);
        cursor.insertText(item.second, textFmt);
    }
}

// ── DB helpers ────────────────────────────────────────────────────────────────

QSqlRecord TicketPrintManager::getShopProfileRecord()
{
    QSqlQuery query(m_db->database());
    query.prepare(
        "SELECT shop_name, tax_id, address, postal, city, phone, email, logo_path, "
        "facebook, instagram, tiktok, whatsapp "
        "FROM shop_profile ORDER BY id DESC LIMIT 1");

    if (query.exec() && query.next())
        return query.record();

    return QSqlRecord();
}

QSqlRecord TicketPrintManager::getTicketRecord(int ticketId)
{
    QSqlQuery query(m_db->database());
    query.prepare(
        "SELECT id, ticket_number, date, time, seller_display_name, "
        "subtotal, discount_amount, total, status "
        "FROM tickets WHERE id = ?");
    query.addBindValue(ticketId);

    if (query.exec() && query.next())
        return query.record();

    return QSqlRecord();
}

QList<QSqlRecord> TicketPrintManager::getTicketItemsRecord(int ticketId)
{
    QList<QSqlRecord> items;
    QSqlQuery query(m_db->database());
    query.prepare(
        "SELECT product_name, variant_name, quantity, unit_price, total_price "
        "FROM ticket_items WHERE ticket_id = ? ORDER BY id");
    query.addBindValue(ticketId);

    if (query.exec()) {
        while (query.next())
            items.append(query.record());
    }

    return items;
}

// ── Number to French words ────────────────────────────────────────────────────

QString TicketPrintManager::numberToFrenchWords(double amount)
{
    static const QString units[] = { "", "un", "deux", "trois", "quatre", "cinq",
                                    "six", "sept", "huit", "neuf" };
    static const QString teens[] = { "dix", "onze", "douze", "treize", "quatorze",
                                    "quinze", "seize", "dix-sept", "dix-huit", "dix-neuf" };
    static const QString tens[]  = { "", "dix", "vingt", "trente", "quarante", "cinquante",
                                   "soixante", "soixante", "quatre-vingt", "quatre-vingt-dix" };

    int dinars  = static_cast<int>(amount);
    int millimes = static_cast<int>(qRound((amount - dinars) * 1000));

    QString dinarPart, millimePart;

    auto convertPart = [&](int value, const QString& singular, const QString& plural) -> QString {
        if (value == 0) return "";
        if (value == 1) return QString("un ") + singular;
        QString result;
        if (value < 10) {
            result = units[value];
        } else if (value < 20) {
            result = teens[value - 10];
        } else if (value < 100) {
            int ten  = value / 10;
            int unit = value % 10;
            if (ten == 7 || ten == 9) {
                result = tens[ten] + "-" + teens[unit];
            } else {
                result = tens[ten];
                if (unit > 0)
                    result += (ten == 8 && unit == 1) ? "-un" : "-" + units[unit];
            }
        }
        return result + " " + (value == 1 ? singular : plural);
    };

    dinarPart   = convertPart(dinars,   "dinar",   "dinars");
    millimePart = convertPart(millimes, "millime", "millimes");

    QString result;
    if (!dinarPart.isEmpty() && !millimePart.isEmpty())
        result = dinarPart + " et " + millimePart;
    else if (!dinarPart.isEmpty())
        result = dinarPart;
    else if (!millimePart.isEmpty())
        result = millimePart;
    else
        result = "zéro dinars";

    if (!result.isEmpty())
        result[0] = result[0].toUpper();

    return result;
}

QString TicketPrintManager::formatCurrency(double amount)
{
    return QString("%1 TND").arg(amount, 0, 'f', 3);
}

// ── Text format helpers ───────────────────────────────────────────────────────

QTextCharFormat TicketPrintManager::getTitleFormat()
{
    QTextCharFormat f;
    f.setFont(QFont("Arial", 20, QFont::Bold));
    f.setForeground(QBrush(Qt::black));
    return f;
}

QTextCharFormat TicketPrintManager::getHeaderFormat()
{
    QTextCharFormat f;
    f.setFont(QFont("Arial", 12, QFont::Bold));
    f.setForeground(QBrush(QColor("#2F5496")));
    return f;
}

QTextCharFormat TicketPrintManager::getNormalFormat()
{
    QTextCharFormat f;
    f.setFont(QFont("Arial", 10));
    f.setForeground(QBrush(Qt::black));
    return f;
}

QTextCharFormat TicketPrintManager::getBoldFormat()
{
    QTextCharFormat f;
    f.setFont(QFont("Arial", 10, QFont::Bold));
    f.setForeground(QBrush(Qt::black));
    return f;
}

QTextCharFormat TicketPrintManager::getCompanyNameFormat()
{
    QTextCharFormat f;
    f.setFont(QFont("Arial", 14, QFont::Bold));
    f.setForeground(QBrush(QColor("#2F5496")));
    return f;
}

QTextBlockFormat TicketPrintManager::getCenterFormat()
{
    QTextBlockFormat f;
    f.setAlignment(Qt::AlignCenter);
    return f;
}

QTextBlockFormat TicketPrintManager::getLeftFormat()
{
    QTextBlockFormat f;
    f.setAlignment(Qt::AlignLeft);
    return f;
}

QTextBlockFormat TicketPrintManager::getRightFormat()
{
    QTextBlockFormat f;
    f.setAlignment(Qt::AlignRight);
    return f;
}
