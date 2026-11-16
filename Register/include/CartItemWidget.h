#ifndef CARTITEMWIDGET_H
#define CARTITEMWIDGET_H

#include <QWidget>

class QLabel;
class QSpinBox;
class QPushButton;

/**
 * Custom widget for displaying a cart item
 * Shows: Product name, quantity spinbox, price, remove button
 */
class CartItemWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CartItemWidget(int cartKey, const QString& name, int quantity,
                            double unitPrice, int maxQuantity, QWidget* parent = nullptr);

    // Getters
    int cartKey() const { return m_cartKey; }
    int quantity() const;

    // Update display
    void updatePrice(double totalPrice);

signals:
    void quantityChanged(int cartKey, int newQuantity);
    void removeRequested(int cartKey);

private:
    void setupUI(const QString& name, int quantity, double unitPrice, int maxQuantity);

    int m_cartKey;

    // UI elements
    QLabel* m_nameLabel;
    QSpinBox* m_quantitySpin;
    QLabel* m_priceLabel;
    QPushButton* m_removeBtn;
};

#endif // CARTITEMWIDGET_H
