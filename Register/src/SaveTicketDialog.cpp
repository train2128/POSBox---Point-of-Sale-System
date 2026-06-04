#include "SaveTicketDialog.h"
#include "Prefixes.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QTextEdit>
#include <QPushButton>
#include <QDialogButtonBox>

SaveTicketDialog::SaveTicketDialog(QWidget* parent)
    : QDialog(parent)
    , m_nameField(nullptr)
    , m_advanceSpin(nullptr)
    , m_descField(nullptr)
{
    setWindowTitle("Save Open Ticket");
    setModal(true);
    setMinimumWidth(340);
    setupUI();
}

void SaveTicketDialog::setupUI()
{
    QVBoxLayout* main = new QVBoxLayout(this);
    main->setContentsMargins(16, 16, 16, 16);
    main->setSpacing(12);

    // Title
    QLabel* title = new QLabel("Ticket Details", this);
    title->setAlignment(Qt::AlignCenter);
    main->addWidget(title);

    // Form
    QFormLayout* form = new QFormLayout();
    form->setSpacing(8);

    m_nameField = new QLineEdit(this);
    m_nameField->setPlaceholderText("Customer name (optional)");
    form->addRow("Customer Name:", m_nameField);

    m_advanceSpin = new QDoubleSpinBox(this);
    m_advanceSpin->setMinimum(0.0);
    m_advanceSpin->setMaximum(999999.0);
    m_advanceSpin->setDecimals(3);
    m_advanceSpin->setSuffix(QString(" %1").arg(AppConstants::CURRENCY));
    m_advanceSpin->setValue(0.0);
    form->addRow("Advance Payment:", m_advanceSpin);

    m_descField = new QTextEdit(this);
    m_descField->setPlaceholderText("Notes or description (optional)");
    m_descField->setFixedHeight(72);
    form->addRow("Description:", m_descField);

    main->addLayout(form);

    // Buttons
    QDialogButtonBox* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText("Save Ticket");
    buttons->button(QDialogButtonBox::Cancel)->setText("Cancel");

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    main->addWidget(buttons);
}

// ── Pre-fill ──────────────────────────────────────────────────────────────────

void SaveTicketDialog::setCustomerName(const QString& name)
{
    if (m_nameField) m_nameField->setText(name);
}

void SaveTicketDialog::setAdvancePayment(double amount)
{
    if (m_advanceSpin) m_advanceSpin->setValue(amount);
}

void SaveTicketDialog::setDescription(const QString& desc)
{
    if (m_descField) m_descField->setPlainText(desc);
}

// ── Getters ───────────────────────────────────────────────────────────────────

QString SaveTicketDialog::customerName() const
{
    return m_nameField ? m_nameField->text().trimmed() : QString();
}

double SaveTicketDialog::advancePayment() const
{
    return m_advanceSpin ? m_advanceSpin->value() : 0.0;
}

QString SaveTicketDialog::description() const
{
    return m_descField ? m_descField->toPlainText().trimmed() : QString();
}
