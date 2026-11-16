#include "ProductButton.h"

ProductButton::ProductButton(QWidget* parent)
    : QFrame(parent)
{
    setObjectName("product-button");
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_Hover, true);
}

void ProductButton::mousePressEvent(QMouseEvent* event)
{
    QFrame::mousePressEvent(event);
    emit clicked();
}
