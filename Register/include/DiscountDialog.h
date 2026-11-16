#ifndef DISCOUNTDIALOG_H
#define DISCOUNTDIALOG_H

#include "DiscountHandler.h"
#include "ModernDialog.h"
#include <QList>

class QButtonGroup;
class QLineEdit;
class QLabel;
class LocalApi;

/**
 * Dialog for applying a discount to the cart.
 * Supports preset discounts from DB and custom fixed/percentage input.
 * Only one discount can be active at a time — replaces any existing discount.
 */
class DiscountDialog : public ModernDialog
{
    Q_OBJECT

public:
    explicit DiscountDialog(LocalApi* api, double subtotal, QWidget* parent = nullptr);
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
    struct Preset {
        int id;  // -1 is "None"
        QString name;
        QString type;
        double value;
    };

    void setupUI();
    void loadPresets();
    void updatePreview();
    const Preset& selectedPreset() const;
    static QString chipText(const Preset& preset);

    LocalApi*    m_api;
    double        m_subtotal;
    QList<Preset> m_presets;
    QButtonGroup* m_presetGroup;
    QLineEdit*    m_customEdit;
    QLabel*       m_discountName;
    QLabel*       m_discountValue;
    QLabel*       m_totalLabel;
    DiscountItem  m_result;
};

#endif // DISCOUNTDIALOG_H
