#ifndef CUSTOMITEMDIALOG_H
#define CUSTOMITEMDIALOG_H

#include <QDialog>
#include <QString>

class QLineEdit;

/**
 * Dialog for adding a custom (non-inventory) item to the cart.
 * Collects name, cost, and price from the user.
 */
class CustomItemDialog : public QDialog
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

private:
    void setupUI();

    QLineEdit* m_nameEdit;
    QLineEdit* m_costEdit;
    QLineEdit* m_priceEdit;
};

#endif // CUSTOMITEMDIALOG_H
