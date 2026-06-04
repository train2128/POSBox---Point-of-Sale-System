#ifndef CUSTOMERINVOICEDIALOG_H
#define CUSTOMERINVOICEDIALOG_H

#include <QDialog>
#include "TicketPrintManager.h"

namespace Ui { class CustomerInvoiceDialog; }

class CustomerInvoiceDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CustomerInvoiceDialog(QWidget* parent = nullptr);
    ~CustomerInvoiceDialog();
    CustomerInvoiceData getCustomerData() const;

private:
    Ui::CustomerInvoiceDialog* ui;
};

#endif // CUSTOMERINVOICEDIALOG_H