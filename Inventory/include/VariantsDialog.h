#ifndef VARIANTSDIALOG_H
#define VARIANTSDIALOG_H

#include "ModernDialog.h"
#include <QList>

class QFrame;
class QLabel;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QVBoxLayout;
class LocalApi;
class QLabel;
class QLineEdit;
class QPushButton;
class QWidget;


/**
 * In-memory variant data — passed in and returned by VariantsDialog.
 * ProductDialog owns this list and is responsible for saving to DB.
 */
struct PendingVariant {
    int     variantId;  // -1 = new, > 0 = existing (edit mode)
    QString name;
    double  cost;
    double  price;
    int     stock;      // -1 = use parent service tracking

    PendingVariant() : variantId(-1), cost(0.0), price(0.0), stock(0) {}
    PendingVariant(int id, const QString& n, double c, double p, int s)
        : variantId(id), name(n), cost(c), price(p), stock(s) {}
};

/**
 * Variants management dialog — list + detail editor (proposal 3).
 *
 * Left card  : clickable variant rows (name + price), "+ Add variant" pill.
 * Right card : detail editor for the selected variant (name, cost/price,
 *              live margin chip, stock, "Delete variant" pill).
 *
 * Does NOT touch the database directly — all saving is deferred to ProductDialog.
 *
 * Usage:
 *   VariantsDialog dlg(m_api, m_pendingVariants, productId, isService, name, this);
 *   if (dlg.exec() == QDialog::Accepted)
 *       m_pendingVariants = dlg.getPendingVariants();
 */
class VariantsDialog : public ModernDialog
{
    Q_OBJECT

public:
    /**
     * @param api          LocalApi (used only in edit mode to load existing variants)
     * @param pending      Existing pending variants to pre-populate the dialog
     * @param productId    > 0 in edit mode (loads from the database if pending is empty), -1 in add mode
     * @param isService    Whether parent product is a service (hides the stock field)
     * @param productName  Parent product name, shown in the subtitle
     * @param parent
     */
    explicit VariantsDialog(LocalApi* api,
                            const QList<PendingVariant>& pending,
                            int productId,
                            bool isService,
                            const QString& productName,
                            QWidget* parent = nullptr);

    /** Returns the validated variant list after the dialog is accepted. */
    QList<PendingVariant> getPendingVariants() const { return m_pendingVariants; }

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
    void showEvent(QShowEvent* event) override;

private slots:
    void onAddVariant();
    void onDeleteVariant();
    void onAccepted();
    void onDetailEdited();

private:
    /** One row of the variant list — raw texts are the source of truth. */
    struct VariantRow {
        int          variantId = -1;
        QString      nameText;
        QString      costText;
        QString      priceText;
        QString      stockText;
        QFrame*      container  = nullptr;
        QWidget*     header     = nullptr;
        QLabel*      nameLabel  = nullptr;
        QLabel*      priceLabel = nullptr;
        QLabel*      stockLabel = nullptr;
    };

    void setupUI();
    void setupConnections();
    void loadExistingVariants();
    void populateFromPending(const QList<PendingVariant>& pending);
    void appendRow(const VariantRow& row);
    void removeRowAt(int index);
    void selectRow(int index);
    void clearDetail();
    void refreshDetailChip();
    void updateSubtitle();
    void updateRowLabels(VariantRow& row);
    void updateListHeight();
    QWidget* buildEditor();
    bool validateAndCollect();

    QScrollArea* m_scroll = nullptr;
    QWidget*     m_listWidget = nullptr;
    QVBoxLayout* m_listLayout = nullptr;
    QWidget*     m_editor = nullptr;
    QLineEdit*   m_nameEdit = nullptr;
    QLineEdit*   m_costEdit = nullptr;
    QLineEdit*   m_priceEdit = nullptr;
    QLineEdit*   m_stockEdit = nullptr;
    QWidget*     m_stockColumn = nullptr;
    QLabel*      m_marginValue = nullptr;
    QPushButton* m_deleteBtn = nullptr;
    LocalApi*            m_api;
    int                   m_productId;
    bool                  m_isService;
    QString               m_productName;
    QList<VariantRow>     m_rows;
    int                   m_selected = -1;
    QList<PendingVariant> m_pendingVariants;
};

#endif // VARIANTSDIALOG_H
