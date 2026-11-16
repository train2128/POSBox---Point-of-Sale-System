#include "CartItemWidget.h"
#include "ElidedLabel.h"
#include "RegisterConstants.h"
#include "Prefixes.h"
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QSpinBox>
#include <QPushButton>
#include <QStyle>

CartItemWidget::CartItemWidget(int cartKey, const QString& name, int quantity,
                               double unitPrice, int maxQuantity, QWidget* parent)
    : QWidget(parent)
    , m_cartKey(cartKey)
    , m_nameLabel(nullptr)
    , m_quantitySpin(nullptr)
    , m_priceLabel(nullptr)
    , m_removeBtn(nullptr)
{
    setupUI(name, quantity, unitPrice, maxQuantity);
}

void CartItemWidget::setupUI(const QString& name, int quantity, double unitPrice, int maxQuantity)
{
    setObjectName("cart-item-widget");
    setProperty("class", "cart-item-widget");

    setAttribute(Qt::WA_StyledBackground, true);

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 6, 6, 6);
    layout->setSpacing(RegisterConstants::CART_ITEM_SPACING);

    // Name, with the unit price under it
    QVBoxLayout* nameColumn = new QVBoxLayout;
    nameColumn->setContentsMargins(0, 0, 0, 0);
    nameColumn->setSpacing(0);

    m_nameLabel = new ElidedLabel(name, this);
    m_nameLabel->setObjectName("cart-item-name");
    m_nameLabel->setProperty("class", "cart-item-name");
    m_nameLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_nameLabel->setToolTip(name);
    nameColumn->addWidget(m_nameLabel);

    QLabel* unitLabel = new QLabel(tr("%1 each").arg(AppHelpers::formatAmount(unitPrice)), this);
    unitLabel->setObjectName("cart-item-unit");
    nameColumn->addWidget(unitLabel);

    // Quantity: the spin box keeps the value and the limits, the buttons step it
    m_quantitySpin = new QSpinBox(this);
    m_quantitySpin->setObjectName("cart-item-quantity");
    m_quantitySpin->setProperty("class", "cart-item-quantity");
    m_quantitySpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_quantitySpin->setAlignment(Qt::AlignCenter);
    m_quantitySpin->setFixedWidth(RegisterConstants::CART_QUANTITY_SPINBOX_WIDTH);
    m_quantitySpin->setMinimum(1);
    m_quantitySpin->setMaximum(maxQuantity);
    m_quantitySpin->setValue(quantity);

    connect(m_quantitySpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this, unitPrice](int newQty) {
                updatePrice(unitPrice * newQty);
                emit quantityChanged(m_cartKey, newQty);
            });

    auto stepButton = [this](const QString& text, int step) {
        auto* button = new QPushButton(text, this);
        button->setObjectName("cart-step-button");
        button->setFixedSize(RegisterConstants::CART_STEP_BUTTON_SIZE,
                             RegisterConstants::CART_STEP_BUTTON_SIZE);
        button->setFocusPolicy(Qt::NoFocus);
        connect(button, &QPushButton::clicked, this,
                [this, step]() { m_quantitySpin->stepBy(step); });
        return button;
    };

    QFrame* stepper = new QFrame(this);
    stepper->setObjectName("cart-stepper");
    QHBoxLayout* stepperLayout = new QHBoxLayout(stepper);
    stepperLayout->setContentsMargins(1, 1, 1, 1);
    stepperLayout->setSpacing(0);
    stepperLayout->addWidget(stepButton(QStringLiteral("−"), -1));
    stepperLayout->addWidget(m_quantitySpin);
    stepperLayout->addWidget(stepButton(QStringLiteral("+"), 1));

    // Line total
    m_priceLabel = new QLabel(AppHelpers::formatAmount(unitPrice * quantity), this);
    m_priceLabel->setObjectName("cart-item-price");
    m_priceLabel->setProperty("class", "cart-item-price");
    m_priceLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    // Remove button
    m_removeBtn = new QPushButton("×", this);
    m_removeBtn->setObjectName("cart-item-remove");
    m_removeBtn->setProperty("class", "cart-item-remove");
    m_removeBtn->setFixedSize(RegisterConstants::CART_REMOVE_BUTTON_SIZE,
                              RegisterConstants::CART_REMOVE_BUTTON_SIZE);
    m_removeBtn->style()->polish(m_removeBtn);

    connect(m_removeBtn, &QPushButton::clicked,
            this, [this]() {
                emit removeRequested(m_cartKey);
            });

    // Add widgets to layout
    layout->addLayout(nameColumn, 1);
    layout->addWidget(stepper, 0);
    layout->addWidget(m_priceLabel, 0);
    layout->addWidget(m_removeBtn, 0);
}

int CartItemWidget::quantity() const
{
    return m_quantitySpin ? m_quantitySpin->value() : 0;
}

void CartItemWidget::updatePrice(double totalPrice)
{
    if (m_priceLabel) {
        m_priceLabel->setText(AppHelpers::formatAmount(totalPrice));
    }
}
