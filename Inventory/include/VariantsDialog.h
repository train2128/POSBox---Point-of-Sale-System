#ifndef VARIANTSDIALOG_H
#define VARIANTSDIALOG_H

#include <QDialog>
#include <QList>

class DatabaseManager;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QPushButton;

namespace Ui {
class VarDialog;
}

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
 * Internal row widget references — only used inside VariantsDialog.
 */
struct VariantRow {
    int          variantId;
    QLabel*      nameLabel;
    QLineEdit*   nameEdit;
    QLabel*      costLabel;
    QLineEdit*   costEdit;
    QLabel*      priceLabel;
    QLineEdit*   priceEdit;
    QLabel*      stockLabel;
    QLineEdit*   stockEdit;
    QPushButton* deleteBtn;
    QHBoxLayout* layout;

    VariantRow() : variantId(-1), nameLabel(nullptr), nameEdit(nullptr),
        costLabel(nullptr), costEdit(nullptr), priceLabel(nullptr),
        priceEdit(nullptr), stockLabel(nullptr), stockEdit(nullptr),
        deleteBtn(nullptr), layout(nullptr) {}
};

/**
 * Variants management dialog.
 *
 * Does NOT touch the database directly — all saving is deferred to ProductDialog.
 *
 * Usage:
 *   VariantsDialog dlg(m_db, m_pendingVariants, productId, this);
 *   if (dlg.exec() == QDialog::Accepted)
 *       m_pendingVariants = dlg.getPendingVariants();
 */
class VariantsDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @param db              DatabaseManager (used only in edit mode to load existing variants)
     * @param pending         Existing pending variants to pre-populate the dialog
     * @param productId       > 0 in edit mode (loads from DB if pending is empty), -1 in add mode
     * @param isService      Whether parent product is a service (disables stock field)
     * @param parent
     */
    explicit VariantsDialog(DatabaseManager* db,
                            const QList<PendingVariant>& pending,
                            int productId,
                            bool isService,
                            QWidget* parent = nullptr);
    ~VariantsDialog();

    /** Returns the validated variant list after the dialog is accepted. */
    QList<PendingVariant> getPendingVariants() const { return m_pendingVariants; }

private slots:
    void onAddVariantRow();
    void onDeleteVariantRow();
    void onAccepted();
    void onRejected();

private:
    void setupUI();
    void setupConnections();
    void registerStaticRow();
    void loadExistingVariants();
    void populateFromPending(const QList<PendingVariant>& pending);
    void addVariantRow(int variantId = -1, const QString& name = QString(),
                       double cost = 0.0, double price = 0.0, int stock = 0);
    void removeVariantRow(int index);
    void styleDeleteButton(QPushButton* btn, bool enabled);
    bool validateAndCollect();

    Ui::VarDialog*        ui;
    DatabaseManager*      m_db;
    int                   m_productId;
    bool                m_isService;
    QList<VariantRow>     m_rows;
    QList<PendingVariant> m_pendingVariants;
};

#endif // VARIANTSDIALOG_H
