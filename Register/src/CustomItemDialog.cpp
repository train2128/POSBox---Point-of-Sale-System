#include "CustomItemDialog.h"
#include "ModernDialog.h"
#include "Prefixes.h"
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

CustomItemDialog::CustomItemDialog(QWidget* parent)
    : ModernDialog(Look::Card, "edit", Tone::Accent, tr("Custom item"),
                   tr("Sell something that isn't in Inventory"), 420, parent)
    , m_nameEdit(nullptr)
    , m_costEdit(nullptr)
    , m_priceEdit(nullptr)
    , m_profitValue(nullptr)
{
    setObjectName("custom-item-dialog");
    setupUI();
}

CustomItemDialog::~CustomItemDialog() {}

void CustomItemDialog::setupUI()
{
    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText(tr("Item name"));
    addField(tr("Item name"), m_nameEdit);

    m_costEdit = moneyEdit(this);
    m_priceEdit = moneyEdit(this);
    addFieldPair(tr("Cost (optional)"), m_costEdit, tr("Price"), m_priceEdit);

    QLabel* profitLabel = nullptr;
    body()->addWidget(resultStrip(&profitLabel, &m_profitValue));
    profitLabel->setText(tr("Profit on this item"));

    QPushButton* okBtn = addFooter(tr("Add to Cart"), "plus", "add");
    connect(okBtn, &QPushButton::clicked, this, &CustomItemDialog::onOkClicked);
    connect(m_costEdit, &QLineEdit::textChanged, this, &CustomItemDialog::updateProfit);
    connect(m_priceEdit, &QLineEdit::textChanged, this, &CustomItemDialog::updateProfit);

    updateProfit();
    m_nameEdit->setFocus();
}

void CustomItemDialog::updateProfit()
{
    const double price = itemPrice();
    QString text = "-";
    bool loss = false;
    if (price > 0 && !m_costEdit->text().trimmed().isEmpty()) {
        const double profit = price - itemCost();
        loss = profit < 0;
        text = QString("%1  ·  %2%").arg(AppHelpers::formatAmount(profit))
                   .arg(profit / price * 100.0, 0, 'f', 1);
    }
    m_profitValue->setText(text);
    m_profitValue->setProperty("loss", loss);
    m_profitValue->style()->unpolish(m_profitValue);
    m_profitValue->style()->polish(m_profitValue);
}

void CustomItemDialog::onOkClicked()
{
    if (m_nameEdit->text().trimmed().isEmpty()) {
        AppHelpers::showWarning(this, tr("Please enter an item name."));
        m_nameEdit->setFocus();
        return;
    }

    if (m_priceEdit->text().trimmed().isEmpty()) {
        AppHelpers::showWarning(this, tr("Please enter a price."));
        m_priceEdit->setFocus();
        return;
    }

    accept();
}

QString CustomItemDialog::itemName() const
{
    return m_nameEdit->text().trimmed();
}

double CustomItemDialog::itemCost() const
{
    return moneyValue(m_costEdit);
}

double CustomItemDialog::itemPrice() const
{
    return moneyValue(m_priceEdit);
}
