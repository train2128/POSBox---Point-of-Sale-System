#include "SaveTicketDialog.h"
#include "Prefixes.h"
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

namespace {
constexpr int MaxListedLines = 6;

QLabel* summaryLine(QVBoxLayout* into, QWidget* parent, const QString& name, const char* valueStyle)
{
    auto* row = new QHBoxLayout;
    auto* nameLabel = new QLabel(name, parent);
    nameLabel->setObjectName("discountLineName");
    auto* value = new QLabel(parent);
    value->setObjectName(valueStyle);
    value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    row->addWidget(nameLabel, 1);
    row->addWidget(value);
    into->addLayout(row);
    return value;
}

QFrame* divider(QWidget* parent)
{
    auto* line = new QFrame(parent);
    line->setObjectName("modalRowLine");
    line->setFixedHeight(1);
    return line;
}
} // namespace

SaveTicketDialog::SaveTicketDialog(QWidget* parent)
    : ModernDialog(Look::Card, "receipt", Tone::Accent, tr("Save open ticket"),
                   tr("Keep this sale to finish later"), 420, parent)
{
    m_nameField = new QLineEdit(this);
    m_nameField->setPlaceholderText(tr("Optional"));
    addField(tr("Customer name"), m_nameField);

    m_advanceEdit = moneyEdit(this);
    addField(tr("Advance payment"), m_advanceEdit);

    m_descField = new QTextEdit(this);
    m_descField->setObjectName("modalTextBox");
    m_descField->setPlaceholderText(tr("Notes (optional)"));
    m_descField->setTabChangesFocus(true);
    m_descField->setFixedHeight(84);
    addField(tr("Description"), m_descField);

    QVBoxLayout* side = addSidePanel(tr("This ticket"), 270);
    m_lines = new QVBoxLayout;
    m_lines->setSpacing(4);
    side->addLayout(m_lines);
    side->addWidget(divider(this));
    m_totalValue = summaryLine(side, this, tr("Total"), "discountLineValue");
    m_advanceValue = summaryLine(side, this, tr("Advance"), "discountLineValue");
    side->addWidget(divider(this));
    auto* leftCaption = new QLabel(tr("Left to pay"), this);
    leftCaption->setObjectName("discountLineName");
    side->addWidget(leftCaption);
    m_leftValue = new QLabel(this);
    m_leftValue->setObjectName("saveTicketLeft");
    side->addWidget(m_leftValue);
    auto* currency = new QLabel(AppConstants::CURRENCY, this);
    currency->setObjectName("discountLineName");
    side->addWidget(currency);
    side->addStretch(1);

    QPushButton* save = addFooter(tr("Save Ticket"), "check", "add");
    connect(save, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_advanceEdit, &QLineEdit::textChanged, this, [this]() { updateLeftToPay(); });

    updateLeftToPay();
    m_nameField->setFocus();
}

void SaveTicketDialog::setSummary(const QList<Line>& lines, double discount, double total)
{
    m_total = total;
    for (int i = 0; i < lines.size() && i < MaxListedLines; ++i) {
        QLabel* value = summaryLine(m_lines, this, lines.at(i).text, "discountLineValue");
        value->setText(AppHelpers::formatAmount(lines.at(i).amount));
    }
    if (lines.size() > MaxListedLines) {
        auto* more = new QLabel(tr("+ %1 more").arg(lines.size() - MaxListedLines), this);
        more->setObjectName("discountLineName");
        m_lines->addWidget(more);
    }
    if (discount > 0) {
        QLabel* value = summaryLine(m_lines, this, tr("Discount"), "discountLineValue");
        value->setText(QString("− %1").arg(AppHelpers::formatAmount(discount)));
    }
    updateLeftToPay();
}

void SaveTicketDialog::updateLeftToPay()
{
    const double advance = advancePayment();
    m_totalValue->setText(AppHelpers::formatAmount(m_total));
    m_advanceValue->setText(QString("− %1").arg(AppHelpers::formatAmount(advance)));
    m_leftValue->setText(AppHelpers::formatAmount(qMax(0.0, m_total - advance)));
}

// ── Pre-fill ──────────────────────────────────────────────────────────────────

void SaveTicketDialog::setCustomerName(const QString& name)
{
    m_nameField->setText(name);
}

void SaveTicketDialog::setAdvancePayment(double amount)
{
    m_advanceEdit->setText(amount > 0 ? QString::number(amount, 'f', AppConstants::PRICE_DECIMALS) : QString());
}

void SaveTicketDialog::setDescription(const QString& desc)
{
    m_descField->setPlainText(desc);
}

// ── Getters ───────────────────────────────────────────────────────────────────

QString SaveTicketDialog::customerName() const
{
    return m_nameField->text().trimmed();
}

double SaveTicketDialog::advancePayment() const
{
    return moneyValue(m_advanceEdit);
}

QString SaveTicketDialog::description() const
{
    return m_descField->toPlainText().trimmed();
}
