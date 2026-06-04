#ifndef DISCOUNTITEMWIDGET_H
#define DISCOUNTITEMWIDGET_H

#include <QWidget>

class QLabel;
class QPushButton;

/**
 * Custom widget for displaying a discount in the cart
 * Shows: Discount name/value, amount saved, remove button
 */
class DiscountItemWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DiscountItemWidget(int discountIndex, const QString& name,
                                const QString& type, double value, double amount,
                                QWidget* parent = nullptr);

signals:
    void removeRequested(int discountIndex);

private:
    void setupUI(const QString& name, const QString& type, double value, double amount);

    int m_discountIndex;

    // UI elements
    QLabel* m_nameLabel;
    QLabel* m_amountLabel;
    QPushButton* m_removeBtn;
};

#endif // DISCOUNTITEMWIDGET_H
