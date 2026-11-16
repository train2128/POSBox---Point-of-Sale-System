#include "TicketPrintManager.h"
#include "TranslationManager.h"
#include <QCoreApplication>
#include <QStandardPaths>
#include "LocalApi.h"
#include "LocalImages.h"
#include <QDate>
#include <QDebug>
#include <QDir>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPixmap>
#include <QPrintPreviewDialog>
#include <QPrinter>

namespace {

// The painter places everything itself, so the printer adds no margins.
void configureA4(QPrinter& printer)
{
    printer.setPageSize(QPageSize::A4);
    printer.setPageOrientation(QPageLayout::Portrait);
    printer.setPageMargins(QMarginsF(0, 0, 0, 0), QPageLayout::Millimeter);
    printer.setFullPage(true);
}

} // namespace

TicketPrintManager::TicketPrintManager(LocalApi* api, LocalImages* images,
                                       QWidget* parent)
    : QObject(parent)
    , m_api(api)
    , m_images(images)
    , m_parent(parent)
{
}

void TicketPrintManager::printTicketWithPreview(int ticketId)
{
    if (!m_api) {
        qWarning() << "TicketPrintManager: database not available";
        return;
    }

    if (!loadInvoice(ticketId)) {
        qWarning() << "TicketPrintManager: ticket not found:" << ticketId;
        return;
    }

    savePdfToInvoices();

    QPrinter printer(QPrinter::HighResolution);
    configureA4(printer);

    QPrintPreviewDialog preview(&printer, m_parent);
    preview.setWindowTitle(tr("Print Preview - Invoice"));
    preview.resize(900, 700);

    connect(&preview, &QPrintPreviewDialog::paintRequested,
            this, &TicketPrintManager::printPreview);

    preview.exec();
}

void TicketPrintManager::printPreview(QPrinter* printer)
{
    QPainter painter(printer);
    InvoicePainter::paint(painter, m_invoice);
}

bool TicketPrintManager::loadInvoice(int ticketId)
{
    const DataRow ticket = m_api->ticket(ticketId);
    if (ticket.isEmpty())
        return false;

    InvoiceContent invoice;

    invoice.number = ticket.value("ticket_number").toString();
    const QDate ticketDate = QDate::fromString(ticket.value("date").toString(), "yyyy-MM-dd");
    invoice.date = TranslationManager::instance().locale().toString(
        ticketDate.isValid() ? ticketDate : QDate::currentDate(), QLocale::ShortFormat);
    invoice.subtotal = ticket.value("subtotal").toDouble();
    invoice.discount = ticket.value("discount_amount").toDouble();
    invoice.total = ticket.value("total").toDouble();

    const QList<DataRow> items = m_api->select("ticket_items", {{"ticket_id", ticketId}}, "id");
    for (const DataRow& row : items) {
        InvoiceContent::Item item;
        item.name = row.value("product_name").toString();
        item.quantity = row.value("quantity").toInt();
        item.unitPrice = row.value("unit_price").toDouble();
        item.lineTotal = row.value("total_price").toDouble();
        invoice.items.append(item);
    }

    const DataRow shop = m_api->profile();
    invoice.shopName = shop.value("shop_name").toString();
    invoice.shopAddress = shop.value("address").toString();
    invoice.shopCity = shop.value("city").toString();
    invoice.shopPostalCity = (shop.value("postal").toString() + " " + invoice.shopCity).trimmed();

    // The logo file lives in the app-data folder.
    const QString logoFilename = shop.value("logo_path").toString();
    if (!logoFilename.isEmpty() && m_images)
        invoice.logo = m_images->pixmap(logoFilename).toImage();

    const QList<QPair<QString, QString>> contacts{
        {QCoreApplication::translate("Invoice", "Phone: %1"), "phone"},
        {"%1", "email"},
        {tr("Facebook: %1"), "facebook"},
        {tr("Instagram: %1"), "instagram"},
        {tr("TikTok: %1"), "tiktok"},
        {tr("WhatsApp: %1"), "whatsapp"},
    };
    for (const auto& contact : contacts) {
        const QString value = shop.value(contact.second).toString();
        if (!value.isEmpty())
            invoice.footerItems << QString(contact.first).arg(value);
    }

    m_invoice = invoice;
    return true;
}

void TicketPrintManager::savePdfToInvoices()
{
    if (m_invoice.number.isEmpty())
        return;

    // A PDF copy of every invoice, under Documents/POSBox/Invoices.
    const QString invoicesPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                                 + "/POSBox/Invoices/";
    QDir().mkpath(invoicesPath);

    // Named <number>_<DD-MM-YY>.pdf — records when it was generated.
    const QString dateSuffix = QDate::currentDate().toString(QStringLiteral("dd-MM-yy"));
    const QString filePath = invoicesPath + m_invoice.number + "_" + dateSuffix + ".pdf";

    QPrinter printer(QPrinter::HighResolution);
    configureA4(printer);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(filePath);

    QPainter painter(&printer);
    InvoicePainter::paint(painter, m_invoice);
    painter.end();

    qDebug() << "TicketPrintManager: PDF saved to" << filePath;
}
