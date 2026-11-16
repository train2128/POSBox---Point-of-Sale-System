#ifndef INVOICEPAINTER_H
#define INVOICEPAINTER_H

#include <QImage>
#include <QList>
#include <QString>
#include <QStringList>

class QPainter;

// Everything printed on an invoice. Empty fields are left off the page.
struct InvoiceContent {
    struct Item {
        QString name;
        int quantity = 0;
        double unitPrice = 0.0;
        double lineTotal = 0.0;
    };

    QImage logo;
    QString shopName;
    QString shopAddress;
    QString shopPostalCity;
    QString shopCity;
    QStringList footerItems;


    QString number;
    QString date;
    QList<Item> items;
    double subtotal = 0.0;
    double discount = 0.0;
    double total = 0.0;
};

namespace InvoicePainter {

// Draws the invoice on one A4 page. The item rows shrink as needed so the page
// never overflows. The painter's device must be sized to the full paper.
void paint(QPainter& painter, const InvoiceContent& invoice);


} // namespace InvoicePainter

#endif // INVOICEPAINTER_H
