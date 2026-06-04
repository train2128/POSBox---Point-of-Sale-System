#ifndef VARIANTPICKERDIALOG_H
#define VARIANTPICKERDIALOG_H

#include <QDialog>
#include <QList>
#include <QString>

class QListWidget;
class DatabaseManager;

/**
 * Data for a single selectable variant.
 */
struct VariantOption {
    int     id;
    QString name;
    double  price;
    double  cost;
    int     stock;

    VariantOption() : id(-1), price(0.0), cost(0.0), stock(0) {}
    VariantOption(int i, const QString& n, double p, double c, int s)
        : id(i), name(n), price(p), cost(c), stock(s) {}
};

/**
 * Popup dialog for selecting a variant from a product.
 * Fetches variants from DB, shows them in a list with name + price.
 * Returns the selected VariantOption on accept.
 */
class VariantPickerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit VariantPickerDialog(const QString& productName,
                                 int productId,
                                 DatabaseManager* db,
                                 bool isService,
                                 QWidget* parent = nullptr);
    ~VariantPickerDialog();

    /**
     * Returns the variant the user selected.
     * Only valid after exec() == QDialog::Accepted.
     */
    VariantOption selectedVariant() const;

private slots:
    void onOkClicked();
    void onItemDoubleClicked();

private:
    void setupUI(const QString& productName);
    void loadVariants(int productId, bool isService);

    DatabaseManager*       m_db;
    QListWidget*           m_list;
    QList<VariantOption>   m_variants;
    VariantOption          m_selected;
    bool                  m_isService;
};

#endif // VARIANTPICKERDIALOG_H
