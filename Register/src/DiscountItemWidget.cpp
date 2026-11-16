#include "DiscountItemWidget.h"
#include "ElidedLabel.h"
#include "RegisterConstants.h"
#include "Prefixes.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

DiscountItemWidget::DiscountItemWidget(int discountIndex, const QString& name,
                                       const QString& type, double value, double amount,
                                       QWidget* parent)
    : QWidget(parent)
    , m_discountIndex(discountIndex)
    , m_nameLabel(nullptr)
    , m_amountLabel(nullptr)
    , m_removeBtn(nullptr)
{
    setupUI(name, type, value, amount);
}

void DiscountItemWidget::setupUI(const QString& name, const QString& type,
                                 double value, double amount)
{
    setObjectName("discount-item-widget");
    setProperty("class", "discount-item-widget");

    setAttribute(Qt::WA_StyledBackground, true);

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 5, 6, 5);
    layout->setSpacing(10);

    // Build display name with value info
    QString displayName = name;
    if (type == "percentage") {
        displayName += QString(" (%1%)").arg(value, 0, 'f', 1);
    } else {
        displayName += QString(" (%1)").arg(AppHelpers::formatPrice(value));
    }

    // Discount name label
    m_nameLabel = new ElidedLabel(displayName, this);
    m_nameLabel->setToolTip(displayName);
    m_nameLabel->setObjectName("discount-name");
    m_nameLabel->setProperty("class", "discount-name");
    m_nameLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    // Discount amount label (negative)
    m_amountLabel = new QLabel(QString("-%1").arg(AppHelpers::formatPrice(amount)), this);
    m_amountLabel->setObjectName("discount-amount");
    m_amountLabel->setProperty("class", "discount-amount");
    m_amountLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    // Remove button
    m_removeBtn = new QPushButton("×", this);
    m_removeBtn->setObjectName("cart-item-remove");  // Reuse cart remove style
    m_removeBtn->setProperty("class", "cart-item-remove");
    m_removeBtn->setFixedSize(RegisterConstants::CART_REMOVE_BUTTON_SIZE,
                              RegisterConstants::CART_REMOVE_BUTTON_SIZE);

    connect(m_removeBtn, &QPushButton::clicked,
            this, [this]() {
                emit removeRequested(m_discountIndex);
            });

    // Add widgets to layout
    layout->addWidget(m_nameLabel, 1);
    layout->addWidget(m_amountLabel, 0);
    layout->addWidget(m_removeBtn, 0);
}
