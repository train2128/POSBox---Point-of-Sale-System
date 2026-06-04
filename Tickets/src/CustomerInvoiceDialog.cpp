#include "CustomerInvoiceDialog.h"
#include "ui_customerinvoicedialog.h"

CustomerInvoiceDialog::CustomerInvoiceDialog(QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::CustomerInvoiceDialog)
{
    ui->setupUi(this);
    setModal(true);
}

CustomerInvoiceDialog::~CustomerInvoiceDialog()
{
    delete ui;
}

CustomerInvoiceData CustomerInvoiceDialog::getCustomerData() const
{
    CustomerInvoiceData data;
    if (ui && ui->companyNameEdit) data.companyName = ui->companyNameEdit->text().trimmed();
    if (ui && ui->matriculeFiscalEdit) data.matriculeFiscal = ui->matriculeFiscalEdit->text().trimmed();
    if (ui && ui->addressEdit) data.address = ui->addressEdit->text().trimmed();
    if (ui && ui->phoneEdit) data.phone = ui->phoneEdit->text().trimmed();
    return data;
}