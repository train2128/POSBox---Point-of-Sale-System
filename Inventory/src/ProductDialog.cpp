#include "ProductDialog.h"
#include "ui_productdialog.h"
#include "VariantsDialog.h"
#include "InventoryConstants.h"
#include "DatabaseManager.h"
#include "QueryRepository.h"
#include "CommonConfig.h"
#include "Prefixes.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QFileDialog>
#include <QPixmap>
#include <QFile>
#include <QDir>
#include <QIntValidator>
#include <QRegularExpression>
#include <QHBoxLayout>
#include <QDebug>

// ===== CONSTRUCTORS =====

ProductDialog::ProductDialog(DatabaseManager* db, QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::Dialog)
    , m_db(db)
    , m_productId(-1)
    , m_editMode(false)
    , m_imagePath("")
{
    ui->setupUi(this);
    setupUI();
    setupConnections();
    loadCategories();
}

ProductDialog::ProductDialog(DatabaseManager* db, int productId, QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::Dialog)
    , m_db(db)
    , m_productId(productId)
    , m_editMode(true)
    , m_imagePath("")
{
    ui->setupUi(this);
    setupUI();
    setupConnections();
    loadCategories();
    populateForEdit();
}

ProductDialog::~ProductDialog()
{
    delete ui;
}

// ===== SETUP =====

void ProductDialog::setupUI()
{
    setWindowTitle(m_editMode ? tr("Edit Product") : tr("Add Product"));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    // Label texts
    ui->nameLabel->setText(tr("Name"));
    ui->costLabel->setText(tr("Cost"));
    ui->priceLabel->setText(tr("Price"));
    ui->categoryLabel->setText(tr("Category"));
    ui->stockLabel->setText(tr("Stock"));
    ui->hasVariantsCheck->setText(tr("Has Variants"));
    ui->isServiceCheck->setText(tr("Is Service"));
    ui->variantsAddBtn->setText(tr("Manage Variants"));

    // Add left/right margins to labels so they don't hug the edge
    ui->nameLabel->setContentsMargins(8, 0, 8, 0);
    ui->costLabel->setContentsMargins(8, 0, 8, 0);
    ui->priceLabel->setContentsMargins(8, 0, 8, 0);
    ui->categoryLabel->setContentsMargins(8, 0, 8, 0);
    ui->stockLabel->setContentsMargins(8, 0, 8, 0);

    // Center all line edit text
    ui->productName->setAlignment(Qt::AlignCenter);
    ui->costEdit->setAlignment(Qt::AlignCenter);
    ui->priceEdit->setAlignment(Qt::AlignCenter);
    ui->quantityEdit->setAlignment(Qt::AlignCenter);

    // Placeholders
    ui->productName->setPlaceholderText(tr("Product name"));
    ui->costEdit->setPlaceholderText(QString("0.%1").arg(QString(AppConstants::PRICE_DECIMALS, '0')));
    ui->priceEdit->setPlaceholderText(QString("0.%1").arg(QString(AppConstants::PRICE_DECIMALS, '0')));
    ui->quantityEdit->setPlaceholderText("0");

    // Stock: numbers only
    ui->quantityEdit->setValidator(new QIntValidator(0, 9999999, this));

    // Image label — fixed size, clickable
    ui->imageLabel->setText(InventoryConstants::IMAGE_PLACEHOLDER_TEXT);
    ui->imageLabel->setFixedSize(120, 120);
    ui->imageLabel->setStyleSheet(
        "border: 1px solid #505050;"
        "background: #3d3d3d;"
        "color: #808080;"
        "border-radius: 6px;"
        );
    ui->imageLabel->setCursor(Qt::PointingHandCursor);

    // variantsAddBtn disabled by default
    ui->variantsAddBtn->setEnabled(false);

    // Button box labels
    ui->CCBtns->button(QDialogButtonBox::Ok)->setText(tr("Save"));
    ui->CCBtns->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
}

void ProductDialog::setupConnections()
{
    connect(ui->hasVariantsCheck, &QCheckBox::toggled,
            this, &ProductDialog::onHasVariantsChanged);
    connect(ui->isServiceCheck, &QCheckBox::toggled,
            this, &ProductDialog::onIsServiceChanged);
    connect(ui->variantsAddBtn, &QPushButton::clicked,
            this, &ProductDialog::onVariantsAddBtnClicked);
    connect(ui->CCBtns, &QDialogButtonBox::accepted,
            this, &ProductDialog::onAccepted);
    connect(ui->CCBtns, &QDialogButtonBox::rejected,
            this, &ProductDialog::onRejected);

    // Cost/Price formatting on focus-out
    connect(ui->costEdit,  &QLineEdit::editingFinished,
            this, &ProductDialog::onCostEditingFinished);
    connect(ui->priceEdit, &QLineEdit::editingFinished,
            this, &ProductDialog::onPriceEditingFinished);

    // Image label click via event filter
    ui->imageLabel->installEventFilter(this);
}

bool ProductDialog::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == ui->imageLabel && event->type() == QEvent::MouseButtonPress) {
        onImageClicked();
        return true;
    }
    return QDialog::eventFilter(obj, event);
}

void ProductDialog::loadCategories()
{
    if (!m_db) return;

    ui->categorySelect->clear();
    ui->categorySelect->addItem(tr("-- No Category --"), QVariant(-1));

    QSqlQuery query(m_db->database());
    query.prepare(Queries::CATEGORY_SELECT_ALL);
    if (!query.exec()) {
        qWarning() << "Failed to load categories for dialog:" << query.lastError().text();
        return;
    }

    while (query.next()) {
        ui->categorySelect->addItem(query.value("name").toString(), query.value("id").toInt());
    }
}

void ProductDialog::populateForEdit()
{
    if (!m_db || m_productId <= 0) return;

    QSqlQuery query(m_db->database());
    query.prepare(Queries::PRODUCT_SELECT_BY_ID);
    query.addBindValue(m_productId);

    if (!query.exec() || !query.next()) {
        qWarning() << "Failed to load product for edit:" << query.lastError().text();
        return;
    }

    ui->productName->setText(query.value("name").toString());

    // Cost and price with currency formatting
    double cost  = query.value("cost").toDouble();
    double price = query.value("price").toDouble();
    ui->costEdit->setText(AppHelpers::formatPrice(cost));
    ui->priceEdit->setText(AppHelpers::formatPrice(price));

    ui->quantityEdit->setText(QString::number(query.value("stock_quantity").toInt()));
    ui->hasVariantsCheck->setChecked(query.value("has_variants").toBool());
    ui->isServiceCheck->setChecked(query.value("is_service").toBool());

    // Set category
    int categoryId = query.value("category_id").toInt();
    int index = ui->categorySelect->findData(categoryId);
    if (index >= 0) ui->categorySelect->setCurrentIndex(index);

    // Load image
    QString imagePath = query.value("image_path").toString();
    if (!imagePath.isEmpty() && QFile::exists(imagePath)) {
        m_imagePath = imagePath;
        QPixmap pixmap(imagePath);
        ui->imageLabel->setPixmap(pixmap.scaled(ui->imageLabel->size(),
                                                Qt::KeepAspectRatio,
                                                Qt::SmoothTransformation));
    }
}

// ===== SLOTS =====

void ProductDialog::onImageClicked()
{
    QString filePath = QFileDialog::getOpenFileName(
        this, tr("Select Product Image"), QString(),
        tr("Images (*.png *.jpg *.jpeg *.bmp *.gif)"));

    if (filePath.isEmpty()) return;

    m_imagePath = filePath;
    QPixmap pixmap(filePath);
    ui->imageLabel->setPixmap(pixmap.scaled(ui->imageLabel->size(),
                                            Qt::KeepAspectRatio,
                                            Qt::SmoothTransformation));
}

void ProductDialog::onCostEditingFinished()
{
    QString raw = stripCurrency(ui->costEdit->text()).trimmed();
    if (raw.isEmpty()) return;
    bool ok = false;
    double val = raw.toDouble(&ok);
    if (ok) {
        ui->costEdit->setText(AppHelpers::formatPrice(val));
    }
}

void ProductDialog::onPriceEditingFinished()
{
    QString raw = stripCurrency(ui->priceEdit->text()).trimmed();
    if (raw.isEmpty()) return;
    bool ok = false;
    double val = raw.toDouble(&ok);
    if (ok) {
        ui->priceEdit->setText(AppHelpers::formatPrice(val));
    }
}

void ProductDialog::onHasVariantsChanged(bool checked)
{
    ui->variantsAddBtn->setEnabled(checked);
    ui->costEdit->setEnabled(!checked);
    ui->priceEdit->setEnabled(!checked);
    bool isService = ui->isServiceCheck->isChecked();
    ui->quantityEdit->setEnabled(!checked && !isService);
    if (checked) {
        ui->costEdit->clear();
        ui->priceEdit->clear();
    } else if (!isService) {
        ui->quantityEdit->setEnabled(true);
    }
}

void ProductDialog::onIsServiceChanged(bool checked)
{
    ui->quantityEdit->setEnabled(!checked);
    if (checked) {
        ui->quantityEdit->clear();
    }
}

void ProductDialog::onVariantsAddBtnClicked()
{
    bool isService = ui->isServiceCheck->isChecked();
    VariantsDialog dialog(m_db, m_pendingVariants, m_editMode ? m_productId : -1, isService, this);
    if (dialog.exec() == QDialog::Accepted) {
        m_pendingVariants = dialog.getPendingVariants();
        qDebug() << "ProductDialog: stored" << m_pendingVariants.size() << "pending variants";
    }
}

void ProductDialog::onAccepted()
{
    if (validateAndSave()) {
        accept();
    }
}

void ProductDialog::onRejected()
{
    reject();
}

// ===== HELPERS =====

QString ProductDialog::stripCurrency(const QString& text) const
{
    QString result = text.trimmed();
    // Remove trailing currency code (e.g. " TND", " USD", etc.)
    static QRegularExpression rx("\\s+\\S+$");
    result.remove(rx);
    return result;
}

// ===== VALIDATION & SAVE =====

bool ProductDialog::validateAndSave()
{
    if (ui->productName->text().trimmed().isEmpty()) {
        AppHelpers::showWarning(this, tr(InventoryConstants::MSG_PRODUCT_NAME_EMPTY));
        return false;
    }

    bool isService   = ui->isServiceCheck->isChecked();
    bool hasVariants = ui->hasVariantsCheck->isChecked();

    if (!hasVariants) {
        bool ok = false;
        stripCurrency(ui->costEdit->text()).toDouble(&ok);
        if (!ok) {
            AppHelpers::showWarning(this, tr(InventoryConstants::MSG_PRODUCT_COST_INVALID));
            return false;
        }

        stripCurrency(ui->priceEdit->text()).toDouble(&ok);
        if (!ok) {
            AppHelpers::showWarning(this, tr(InventoryConstants::MSG_PRODUCT_PRICE_INVALID));
            return false;
        }
    }

    if (!isService && !hasVariants) {
        bool ok = false;
        ui->quantityEdit->text().toInt(&ok);
        if (!ok) {
            AppHelpers::showWarning(this, tr(InventoryConstants::MSG_PRODUCT_STOCK_INVALID));
            return false;
        }
    }

    return m_editMode ? updateProduct() : saveProduct();
}

bool ProductDialog::saveProduct()
{
    bool isService   = ui->isServiceCheck->isChecked();
    bool hasVariants = ui->hasVariantsCheck->isChecked();

    double cost  = hasVariants ? 0.0 : stripCurrency(ui->costEdit->text()).toDouble();
    double price = hasVariants ? 0.0 : stripCurrency(ui->priceEdit->text()).toDouble();
    int stock    = isService   ? 0   : ui->quantityEdit->text().toInt();
    int categoryId = ui->categorySelect->currentData().toInt();

    QString savedImagePath;
    if (!m_imagePath.isEmpty()) {
        QString destFolder = POSConfig::getAppDataPath() + InventoryConstants::PRODUCTS_IMAGE_FOLDER;
        QDir().mkpath(destFolder);
        QString fileName = QFileInfo(m_imagePath).fileName();
        QString destPath = destFolder + "/" + fileName;
        if (QFile::copy(m_imagePath, destPath)) {
            savedImagePath = destPath;
        }
    }

    QSqlQuery query(m_db->database());
    query.prepare(Queries::PRODUCT_INSERT);
    query.addBindValue(ui->productName->text().trimmed());
    query.addBindValue(QVariant());
    query.addBindValue(cost);
    query.addBindValue(price);
    query.addBindValue(stock);
    query.addBindValue(!isService);
    query.addBindValue(0);
    query.addBindValue(QVariant());
    query.addBindValue(QVariant());
    query.addBindValue(categoryId > 0 ? QVariant(categoryId) : QVariant());
    query.addBindValue(isService);
    query.addBindValue(hasVariants);
    query.addBindValue(savedImagePath.isEmpty() ? QVariant() : QVariant(savedImagePath));

    if (!query.exec()) {
        AppHelpers::showError(this, tr("Failed to save product: ") + query.lastError().text());
        return false;
    }

    int newProductId = query.lastInsertId().toInt();
    qDebug() << "Product saved successfully, id:" << newProductId;

    // Save pending variants now that we have a product ID
    if (hasVariants && !m_pendingVariants.isEmpty()) {
        for (const PendingVariant& pv : m_pendingVariants) {
            QSqlQuery vq(m_db->database());
            vq.prepare(Queries::VARIANT_INSERT);
            vq.addBindValue(newProductId);
            vq.addBindValue(pv.name);
            vq.addBindValue(pv.cost);
            vq.addBindValue(pv.price);
            vq.addBindValue(pv.stock);
            vq.addBindValue(QVariant()); // barcode
            vq.addBindValue(QVariant()); // sku

            if (!vq.exec()) {
                    AppHelpers::showError(this, tr("Failed to save variant: ") + vq.lastError().text());
                return false;
            }
        }
        qDebug() << "Saved" << m_pendingVariants.size() << "variants for product" << newProductId;
    }

    return true;
}

bool ProductDialog::updateProduct()
{
    bool isService   = ui->isServiceCheck->isChecked();
    bool hasVariants = ui->hasVariantsCheck->isChecked();

    double cost  = hasVariants ? 0.0 : stripCurrency(ui->costEdit->text()).toDouble();
    double price = hasVariants ? 0.0 : stripCurrency(ui->priceEdit->text()).toDouble();
    int stock    = isService   ? 0   : ui->quantityEdit->text().toInt();
    int categoryId = ui->categorySelect->currentData().toInt();

    QString savedImagePath = m_imagePath;
    if (!m_imagePath.isEmpty() && !m_imagePath.contains(InventoryConstants::PRODUCTS_IMAGE_FOLDER)) {
        QString destFolder = POSConfig::getAppDataPath() + InventoryConstants::PRODUCTS_IMAGE_FOLDER;
        QDir().mkpath(destFolder);
        QString fileName = QFileInfo(m_imagePath).fileName();
        QString destPath = destFolder + "/" + fileName;
        if (QFile::copy(m_imagePath, destPath)) {
            savedImagePath = destPath;
        }
    }

    QSqlQuery query(m_db->database());
    query.prepare(Queries::PRODUCT_UPDATE);
    query.addBindValue(ui->productName->text().trimmed());
    query.addBindValue(QVariant());
    query.addBindValue(cost);
    query.addBindValue(price);
    query.addBindValue(stock);
    query.addBindValue(!isService);
    query.addBindValue(0);
    query.addBindValue(QVariant());
    query.addBindValue(QVariant());
    query.addBindValue(categoryId > 0 ? QVariant(categoryId) : QVariant());
    query.addBindValue(isService);
    query.addBindValue(hasVariants);
    query.addBindValue(savedImagePath.isEmpty() ? QVariant() : QVariant(savedImagePath));
    query.addBindValue(m_productId);

    if (!query.exec()) {
        AppHelpers::showError(this, tr("Failed to update product: ") + query.lastError().text());
        return false;
    }

    qDebug() << "Product updated successfully:" << m_productId;

    // Sync variants if the pending list was touched
    if (hasVariants && !m_pendingVariants.isEmpty()) {
        // Collect existing variant IDs still present in the pending list
        QList<int> keptIds;
        for (const PendingVariant& pv : m_pendingVariants) {
            if (pv.variantId > 0) keptIds.append(pv.variantId);
        }

        // Delete variants that were removed (not in the kept list)
        QSqlQuery existingQ(m_db->database());
        existingQ.prepare(Queries::VARIANT_SELECT_BY_PRODUCT);
        existingQ.addBindValue(m_productId);
        if (existingQ.exec()) {
            while (existingQ.next()) {
                int existingId = existingQ.value("id").toInt();
                if (!keptIds.contains(existingId)) {
                    QSqlQuery delQ(m_db->database());
                    delQ.prepare(Queries::VARIANT_DELETE);
                    delQ.addBindValue(existingId);
                    delQ.exec();
                }
            }
        }

        // Insert or update each pending variant
        for (const PendingVariant& pv : m_pendingVariants) {
            if (pv.variantId > 0) {
                // Update existing
                QSqlQuery vq(m_db->database());
                vq.prepare(Queries::VARIANT_UPDATE);
                vq.addBindValue(pv.name);
                vq.addBindValue(pv.cost);
                vq.addBindValue(pv.price);
                vq.addBindValue(pv.stock);
                vq.addBindValue(QVariant()); // barcode
                vq.addBindValue(QVariant()); // sku
                vq.addBindValue(true);       // is_active
                vq.addBindValue(pv.variantId);

                if (!vq.exec()) {
                    AppHelpers::showError(this, tr("Failed to update variant: ") + vq.lastError().text());
                    return false;
                }
            } else {
                // Insert new
                QSqlQuery vq(m_db->database());
                vq.prepare(Queries::VARIANT_INSERT);
                vq.addBindValue(m_productId);
                vq.addBindValue(pv.name);
                vq.addBindValue(pv.cost);
                vq.addBindValue(pv.price);
                vq.addBindValue(pv.stock);
                vq.addBindValue(QVariant()); // barcode
                vq.addBindValue(QVariant()); // sku

                if (!vq.exec()) {
                AppHelpers::showError(this, tr("Failed to save variant: ") + vq.lastError().text());
                    return false;
                }
            }
        }

        qDebug() << "Synced" << m_pendingVariants.size() << "variants for product" << m_productId;
    }

    return true;
}
