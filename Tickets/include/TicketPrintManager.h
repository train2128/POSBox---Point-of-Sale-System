#ifndef TICKETPRINTMANAGER_H
#define TICKETPRINTMANAGER_H

#include <QObject>
#include <QString>
#include "InvoicePainter.h"

class QPrinter;
class LocalApi;
class LocalImages;
class QWidget;

/**
 * Handles ticket invoice generation and printing.
 * Collects the invoice data; InvoicePainter draws it on one A4 page.
 */
class TicketPrintManager : public QObject
{
    Q_OBJECT

public:
    explicit TicketPrintManager(LocalApi* api, LocalImages* images,
                                QWidget* parent = nullptr);

    // Show print preview dialog then print
    void printTicketWithPreview(int ticketId);

private slots:
    void printPreview(QPrinter* printer);

private:
    bool loadInvoice(int ticketId);
    void savePdfToInvoices();

    LocalApi*     m_api;
    LocalImages*  m_images;
    QWidget*       m_parent;
    InvoiceContent m_invoice;
};

#endif // TICKETPRINTMANAGER_H
