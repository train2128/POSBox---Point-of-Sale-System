#ifndef CUSTOMITEMDIALOG_H
#define CUSTOMITEMDIALOG_H

#include "ModernDialog.h"
#include <QString>

class QLabel;
class QLineEdit;

/**
 * Dialog for adding a custom (non-inventory) item to the cart.
 * Collects name, cost, and price from the user.
 */
class CustomItemDialog : public ModernDialog
{
    Q_OBJECT

public:
    explicit CustomItemDialog(QWidget* parent = nullptr);
    ~CustomItemDialog();

    QString itemName()  const;
    double  itemCost()  const;
    double  itemPrice() const;

private slots:
    void onOkClicked();
    void updateProfit();

private:
    void setupUI();

    QLineEdit* m_nameEdit;
    QLineEdit* m_costEdit;
    QLineEdit* m_priceEdit;
    QLabel*    m_profitValue;
};

#endif // CUSTOMITEMDIALOG_H
