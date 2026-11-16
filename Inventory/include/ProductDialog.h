#ifndef PRODUCTDIALOG_H
#define PRODUCTDIALOG_H

#include "ModernDialog.h"
#include <QList>
#include <QVariantMap>
#include "VariantsDialog.h"

class QCheckBox;
class QComboBox;
class QFrame;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class LocalApi;
class LocalImages;


/**
 * Add/Edit product dialog
 * Pass productId = -1 for add mode, valid id for edit mode
 */
class ProductDialog : public ModernDialog
{
    Q_OBJECT

public:
    // Add mode
    explicit ProductDialog(LocalApi* api, LocalImages* images,
                           QWidget* parent = nullptr);
    // Edit mode
    explicit ProductDialog(LocalApi* api, LocalImages* images, int productId,
                           QWidget* parent = nullptr);

    // Everything one save sends; built on the UI thread, sent from a worker.
    struct SaveJob {
        QVariantMap product;
        QList<QVariantMap> variants;
        QList<int> deleteIds;
        QString localImage;   // a picked file to shrink and upload, or empty
        bool edit = false;
    };

public slots:
    void reject() override;   // ignored while a save is running

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void onImageClicked();
    void onHasVariantsChanged(bool checked);
    void onIsServiceChanged(bool checked);
    void onVariantsAddBtnClicked();
    void onAccepted();
    void onCostEditingFinished();
    void onPriceEditingFinished();

private:
    void setupUI();
    void setupConnections();
    void loadCategories();
    void populateForEdit();
    void updateStateUI();
    void updateMarginChip();
    bool validate();
    bool buildNewJob(SaveJob* job);
    bool buildUpdateJob(SaveJob* job);
    // Shrinks and uploads the photo, then saves, off the UI thread.
    void startSave(const SaveJob& job);
    void setSaving(bool saving);
    void setSaveStatus(const QString& text);

    // The image_path to save for a stored image or none; a new file is
    // put in the job instead and uploaded by the save worker.
    QVariant imageValueFor(SaveJob* job) const;

    // Strips the currency suffix and returns raw double string
    QString stripCurrency(const QString& text) const;

    QLineEdit*   m_nameEdit = nullptr;
    QLineEdit*   m_costEdit = nullptr;
    QLineEdit*   m_priceEdit = nullptr;
    QComboBox*   m_categorySelect = nullptr;
    QLabel*      m_stockLabel = nullptr;
    QSpinBox*    m_quantity = nullptr;
    QPushButton* m_variantsBtn = nullptr;
    QLabel*      m_imageLabel = nullptr;
    QFrame*      m_marginStrip = nullptr;
    QLabel*      m_marginValue = nullptr;
    QCheckBox*   m_hasVariants = nullptr;
    QCheckBox*   m_isService = nullptr;
    LocalApi*            m_api;
    LocalImages*         m_images;
    int                   m_productId;
    bool                  m_editMode;
    QString               m_imagePath;        // picked file path or stored image name
    bool                  m_imageStored;  // true when m_imagePath is a stored file name
    QList<PendingVariant> m_pendingVariants;

    QPushButton* m_saveBtn = nullptr;
    QPushButton* m_cancelBtn = nullptr;
    QLabel*      m_saveStatus = nullptr;
    QWidget*     m_spinner = nullptr;
    QString      m_saveText;
    bool         m_saving = false;
};

#endif // PRODUCTDIALOG_H
