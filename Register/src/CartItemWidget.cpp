#include "CartItemWidget.h"
#include "RegisterConstants.h"
#include "Prefixes.h"
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

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(RegisterConstants::CART_ITEM_SPACING);

    // Product name label
    m_nameLabel = new QLabel(name, this);
    m_nameLabel->setObjectName("cart-item-name");
    m_nameLabel->setProperty("class", "cart-item-name");
    m_nameLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    // Quantity spinbox
    m_quantitySpin = new QSpinBox(this);
    m_quantitySpin->setObjectName("cart-item-quantity");
    m_quantitySpin->setProperty("class", "cart-item-quantity");
    m_quantitySpin->setFixedWidth(RegisterConstants::CART_QUANTITY_SPINBOX_WIDTH);
    m_quantitySpin->setMinimum(1);
    m_quantitySpin->setMaximum(maxQuantity);
    m_quantitySpin->setValue(quantity);

    connect(m_quantitySpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this, unitPrice](int newQty) {
                updatePrice(unitPrice * newQty);
                emit quantityChanged(m_cartKey, newQty);
            });

    // Price label
    double totalPrice = unitPrice * quantity;
    m_priceLabel = new QLabel(AppHelpers::formatPrice(totalPrice), this);
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
    layout->addWidget(m_nameLabel, 1);  // Stretch factor 1
    layout->addWidget(m_quantitySpin, 0);
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
        m_priceLabel->setText(AppHelpers::formatPrice(totalPrice));
    }
}
