#include "CustomItemDialog.h"
#include "Prefixes.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QDoubleValidator>
#include <QStyle>

CustomItemDialog::CustomItemDialog(QWidget* parent)
    : QDialog(parent)
    , m_nameEdit(nullptr)
    , m_costEdit(nullptr)
    , m_priceEdit(nullptr)
{
    setObjectName("custom-item-dialog");
    setWindowTitle("Add Custom Item");
    setModal(true);
    setFixedWidth(320);

    setupUI();
}

CustomItemDialog::~CustomItemDialog() {}

void CustomItemDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    // Title
    QLabel* titleLabel = new QLabel("Custom Item", this);
    titleLabel->setObjectName("custom-item-title");
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);

    // Form fields
    QFormLayout* form = new QFormLayout();
    form->setSpacing(8);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setObjectName("custom-item-field");
    m_nameEdit->setPlaceholderText("Item name");

    m_costEdit = new QLineEdit(this);
    m_costEdit->setObjectName("custom-item-field");
    m_costEdit->setPlaceholderText("0.000");
    m_costEdit->setValidator(new QDoubleValidator(0.0, 999999.0, 3, this));

    m_priceEdit = new QLineEdit(this);
    m_priceEdit->setObjectName("custom-item-field");
    m_priceEdit->setPlaceholderText("0.000");
    m_priceEdit->setValidator(new QDoubleValidator(0.0, 999999.0, 3, this));

    QLabel* nameLabel  = new QLabel("Name:",  this);
    QLabel* costLabel  = new QLabel("Cost:",  this);
    QLabel* priceLabel = new QLabel("Price:", this);
    nameLabel->setObjectName("custom-item-label");
    costLabel->setObjectName("custom-item-label");
    priceLabel->setObjectName("custom-item-label");

    form->addRow(nameLabel,  m_nameEdit);
    form->addRow(costLabel,  m_costEdit);
    form->addRow(priceLabel, m_priceEdit);
    mainLayout->addLayout(form);

    // OK / Cancel
    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(8);

    QPushButton* okBtn = new QPushButton("Add to Cart", this);
    okBtn->setObjectName("custom-item-ok");
    okBtn->setProperty("btnStyle", "add");
    okBtn->style()->unpolish(okBtn);
    okBtn->style()->polish(okBtn);

    QPushButton* cancelBtn = new QPushButton("Cancel", this);
    cancelBtn->setObjectName("custom-item-cancel");
    cancelBtn->setProperty("btnStyle", "delete");
    cancelBtn->style()->unpolish(cancelBtn);
    cancelBtn->style()->polish(cancelBtn);

    btnLayout->addWidget(okBtn);
    btnLayout->addWidget(cancelBtn);
    mainLayout->addLayout(btnLayout);

    connect(okBtn,     &QPushButton::clicked, this, &CustomItemDialog::onOkClicked);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    m_nameEdit->setFocus();
}

void CustomItemDialog::onOkClicked()
{
    if (m_nameEdit->text().trimmed().isEmpty()) {
        AppHelpers::showWarning(this, "Please enter an item name.");
        m_nameEdit->setFocus();
        return;
    }

    if (m_priceEdit->text().trimmed().isEmpty()) {
        AppHelpers::showWarning(this, "Please enter a price.");
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
    return m_costEdit->text().toDouble();
}

double CustomItemDialog::itemPrice() const
{
    return m_priceEdit->text().toDouble();
}
