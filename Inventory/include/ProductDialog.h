#ifndef PRODUCTDIALOG_H
#define PRODUCTDIALOG_H

#include <QDialog>
#include <QList>
#include "VariantsDialog.h"

class DatabaseManager;

namespace Ui {
class Dialog;
}

/**
 * Add/Edit product dialog
 * Pass productId = -1 for add mode, valid id for edit mode
 */
class ProductDialog : public QDialog
{
    Q_OBJECT

public:
    // Add mode
    explicit ProductDialog(DatabaseManager* db, QWidget* parent = nullptr);
    // Edit mode
    explicit ProductDialog(DatabaseManager* db, int productId, QWidget* parent = nullptr);
    ~ProductDialog();

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void onImageClicked();
    void onHasVariantsChanged(bool checked);
    void onIsServiceChanged(bool checked);
    void onVariantsAddBtnClicked();
    void onAccepted();
    void onRejected();
    void onCostEditingFinished();
    void onPriceEditingFinished();

private:
    void setupUI();
    void setupConnections();
    void loadCategories();
    void populateForEdit();
    bool validateAndSave();
    bool saveProduct();
    bool updateProduct();

    // Strips " TND" suffix and returns raw double string
    QString stripTND(const QString& text) const;

    Ui::Dialog*           ui;
    DatabaseManager*      m_db;
    int                   m_productId;
    bool                  m_editMode;
    QString               m_imagePath;
    QList<PendingVariant> m_pendingVariants;
};

#endif // PRODUCTDIALOG_H
