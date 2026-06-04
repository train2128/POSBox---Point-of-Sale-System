#ifndef DISCOUNTDIALOG_H
#define DISCOUNTDIALOG_H

#include "DiscountHandler.h"
#include <QDialog>

class QComboBox;
class QLineEdit;
class QLabel;
class DatabaseManager;

/**
 * Dialog for applying a discount to the cart.
 * Supports preset discounts from DB and custom fixed/percentage input.
 * Only one discount can be active at a time — replaces any existing discount.
 */
class DiscountDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DiscountDialog(DatabaseManager* db, double subtotal, QWidget* parent = nullptr);
    ~DiscountDialog();

    /**
     * Returns the discount to apply.
     * Only valid after exec() == QDialog::Accepted.
     */
    DiscountItem result() const;

private slots:
    void onOkClicked();
    void onCustomInputChanged(const QString& text);
    void onPresetChanged(int index);

private:
    void setupUI();
    void loadPresets();
    void updatePreview();

    DatabaseManager* m_db;
    double           m_subtotal;

    QComboBox* m_presetCombo;
    QLineEdit* m_customEdit;
    QLabel*    m_previewLabel;

    DiscountItem m_result;
};

#endif // DISCOUNTDIALOG_H
