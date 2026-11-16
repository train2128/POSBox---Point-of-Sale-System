#include "ProductDialog.h"
#include "ToggleSwitch.h"
#include "VariantsDialog.h"
#include "InventoryConstants.h"
#include "LocalApi.h"
#include "LocalImages.h"
#include "CommonConfig.h"
#include "Prefixes.h"
#include <QComboBox>
#include <QEvent>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QPixmap>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QPushButton>
#include <QStyle>
#include <QDebug>
#include <QBuffer>
#include <QImageReader>
#include <QPainter>
#include <QPointer>
#include <QThread>
#include <QTimer>
#include <QUuid>
#include <memory>
#include "MessageDialog.h"
#include "LineIcons.h"

// ===== CONSTRUCTORS =====

namespace {

// A small spinning circle shown inside the Save button while saving.
class SaveSpinner final : public QWidget
{
public:
    explicit SaveSpinner(QWidget* parent) : QWidget(parent)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setFixedSize(16, 16);
        m_timer.setInterval(40);
        connect(&m_timer, &QTimer::timeout, this, [this] { m_angle = (m_angle + 30) % 360; update(); });
    }
    void start() { m_timer.start(); show(); raise(); }
    void stop() { m_timer.stop(); hide(); }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QRectF r = QRectF(rect()).adjusted(2, 2, -2, -2);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(255, 255, 255, 70), 2.2));
        p.drawEllipse(r);
        p.setPen(QPen(Qt::white, 2.2, Qt::SolidLine, Qt::RoundCap));
        p.drawArc(r, -m_angle * 16, 100 * 16);
    }

private:
    QTimer m_timer;
    int m_angle = 0;
};

constexpr int MaxPhotoSide = 800;

// Reads the picked photo (turned upright), shrinks it to MaxPhotoSide and
// encodes it: PNG when it has transparency, JPEG otherwise.
bool shrinkPhoto(const QString& path, QByteArray* data, QString* extension)
{
    QImageReader reader(path);
    reader.setAutoTransform(true);
    QImage image = reader.read();
    if (image.isNull()) return false;
    if (image.width() > MaxPhotoSide || image.height() > MaxPhotoSide)
        image = image.scaled(MaxPhotoSide, MaxPhotoSide, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    const bool alpha = image.hasAlphaChannel();
    QBuffer buffer(data);
    buffer.open(QIODevice::WriteOnly);
    if (alpha) {
        if (!image.save(&buffer, "PNG")) return false;
        *extension = "png";
    } else {
        if (!image.convertToFormat(QImage::Format_RGB32).save(&buffer, "JPG", 85)) return false;
        *extension = "jpg";
    }
    return !data->isEmpty();
}

struct SaveResult {
    int id = -1;
    QString error;
    QString imageName;
    QByteArray imageData;
    QString warning;
};

} // namespace

ProductDialog::ProductDialog(LocalApi* api, LocalImages* images, QWidget* parent)
    : ModernDialog(Look::Card, "box", Tone::Accent, tr("Add product"),
                   tr("Shows in Register and Inventory"), 420, parent)
    , m_api(api)
    , m_images(images)
    , m_productId(-1)
    , m_editMode(false)
    , m_imagePath("")
    , m_imageStored(false)
{
    setupUI();
    setupConnections();
    loadCategories();
}

ProductDialog::ProductDialog(LocalApi* api, LocalImages* images, int productId,
                             QWidget* parent)
    : ModernDialog(Look::Card, "box", Tone::Accent, tr("Edit product"),
                   tr("Shows in Register and Inventory"), 420, parent)
    , m_api(api)
    , m_images(images)
    , m_productId(productId)
    , m_editMode(true)
    , m_imagePath("")
    , m_imageStored(false)
{
    setupUI();
    setupConnections();
    loadCategories();
    populateForEdit();
}

// ===== SETUP =====

void ProductDialog::setupUI()
{
    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText(tr("Product name"));
    addField(tr("Name"), m_nameEdit);

    m_costEdit = moneyEdit(this);
    m_priceEdit = moneyEdit(this);
    addFieldPair(tr("Cost"), m_costEdit, tr("Price"), m_priceEdit);

    // Quantity, or the variants button when the product has variants. The
    // column stays in place so the window doesn't jump.
    m_categorySelect = new QComboBox(this);
    auto* stockColumn = new QWidget(this);
    auto* stockLayout = new QVBoxLayout(stockColumn);
    stockLayout->setContentsMargins(0, 0, 0, 0);
    m_quantity = new QSpinBox(stockColumn);
    m_quantity->setRange(0, 9999999);
    m_variantsBtn = new QPushButton(tr("Manage variants"), stockColumn);
    m_variantsBtn->setProperty("btnStyle", "edit");
    m_variantsBtn->setAutoDefault(false);
    m_variantsBtn->setCursor(Qt::PointingHandCursor);
    stockLayout->addWidget(m_quantity);
    stockLayout->addWidget(m_variantsBtn);
    addFieldPair(tr("Category"), m_categorySelect, tr("Quantity"), stockColumn);
    for (QLabel* label : findChildren<QLabel*>("modalFieldLabel")) {
        if (label->text() == tr("Quantity"))
            m_stockLabel = label;
    }

    // Square photo picker, clickable, beside the live margin
    auto* photoRow = new QHBoxLayout;
    photoRow->setSpacing(12);
    m_imageLabel = new QLabel("+", this);
    m_imageLabel->setObjectName("imageLabel");
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setFixedSize(76, 76);
    m_imageLabel->setCursor(Qt::PointingHandCursor);
    m_imageLabel->setToolTip(InventoryConstants::IMAGE_PLACEHOLDER_TEXT);
    QLabel* marginCaption = nullptr;
    m_marginStrip = resultStrip(&marginCaption, &m_marginValue);
    marginCaption->setText(tr("Margin"));
    photoRow->addWidget(m_imageLabel);
    photoRow->addWidget(m_marginStrip, 1, Qt::AlignVCenter);
    body()->addLayout(photoRow);

    const auto switchRow = [this](const QString& text) {
        auto* row = new QHBoxLayout;
        auto* label = new QLabel(text, this);
        label->setObjectName("modalSwitchLabel");
        auto* toggle = new ToggleSwitch(this);
        row->addWidget(label, 1);
        row->addWidget(toggle);
        body()->addLayout(row);
        return toggle;
    };
    m_hasVariants = switchRow(tr("Has variants"));
    m_isService = switchRow(tr("Is a service (no stock)"));

    QPushButton* save = addFooter(m_editMode ? tr("Save") : tr("Save Product"), "check", "add");
    connect(save, &QPushButton::clicked, this, &ProductDialog::onAccepted);
    m_saveBtn = save;
    m_saveText = save->text();
    for (QPushButton* b : findChildren<QPushButton*>())
        if (b != save && b->text() == tr("Cancel")) m_cancelBtn = b;
    // What the save is doing right now, on the left of the footer.
    m_saveStatus = new QLabel(this);
    m_saveStatus->setObjectName("modalSubtitle");
    m_saveStatus->hide();
    if (auto* footer = save->parentWidget() ? qobject_cast<QHBoxLayout*>(save->parentWidget()->layout()) : nullptr)
        footer->insertWidget(0, m_saveStatus);
    m_spinner = new SaveSpinner(save);
    m_spinner->hide();

    // Initial switch-driven state + live margin
    updateStateUI();
    updateMarginChip();
    m_nameEdit->setFocus();
}

void ProductDialog::setupConnections()
{
    connect(m_hasVariants, &QCheckBox::toggled,
            this, &ProductDialog::onHasVariantsChanged);
    connect(m_isService, &QCheckBox::toggled,
            this, &ProductDialog::onIsServiceChanged);
    connect(m_variantsBtn, &QPushButton::clicked,
            this, &ProductDialog::onVariantsAddBtnClicked);

    // Cost/Price formatting on focus-out
    connect(m_costEdit,  &QLineEdit::editingFinished,
            this, &ProductDialog::onCostEditingFinished);
    connect(m_priceEdit, &QLineEdit::editingFinished,
            this, &ProductDialog::onPriceEditingFinished);

    // Live margin chip under Cost/Price
    connect(m_costEdit,  &QLineEdit::textChanged,
            this, &ProductDialog::updateMarginChip);
    connect(m_priceEdit, &QLineEdit::textChanged,
            this, &ProductDialog::updateMarginChip);

    // Image label click via event filter
    m_imageLabel->installEventFilter(this);

    // Product images arrive asynchronously from the LocalImages worker.
    if (m_images) {
        connect(m_images, &LocalImages::pixmapReady, this,
                [this](const QString& filename, int maxSize, const QImage& image) {
            Q_UNUSED(maxSize)
            if (image.isNull() || !m_imageStored || filename != m_imagePath)
                return;
            m_imageLabel->setPixmap(QPixmap::fromImage(image).scaled(
                m_imageLabel->size(), Qt::KeepAspectRatio,
                Qt::SmoothTransformation));
        });
    }
}

bool ProductDialog::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == m_imageLabel && event->type() == QEvent::MouseButtonPress) {
        onImageClicked();
        return true;
    }
    return ModernDialog::eventFilter(obj, event);
}

void ProductDialog::loadCategories()
{
    if (!m_api) return;

    m_categorySelect->clear();
    m_categorySelect->addItem(tr("-- No Category --"), QVariant(-1));

    const QList<DataRow> categories = m_api->categories();
    if (!m_api->ok()) {
        qWarning() << "Failed to load categories for dialog:" << m_api->lastError();
        return;
    }

    for (const DataRow& c : categories)
        m_categorySelect->addItem(c.value("name").toString(), c.value("id").toInt());
}

void ProductDialog::populateForEdit()
{
    if (!m_api || m_productId <= 0) return;

    const QList<DataRow> rows = m_api->select("products", {{"id", m_productId}});
    if (rows.isEmpty()) {
        qWarning() << "Failed to load product for edit:" << m_api->lastError();
        return;
    }
    const DataRow& p = rows.first();

    m_nameEdit->setText(p.value("name").toString());

    // Cost and price with the currency's decimals
    double cost  = p.value("cost").toDouble();
    double price = p.value("price").toDouble();
    m_costEdit->setText(QString::number(cost, 'f', AppConstants::PRICE_DECIMALS));
    m_priceEdit->setText(QString::number(price, 'f', AppConstants::PRICE_DECIMALS));

    m_quantity->setValue(p.value("stock_quantity").toInt());
    m_hasVariants->setChecked(p.value("has_variants").toInt() != 0);
    m_isService->setChecked(p.value("is_service").toInt() != 0);

    // Set category
    int categoryId = p.value("category_id").toInt();
    int index = m_categorySelect->findData(categoryId);
    if (index >= 0) m_categorySelect->setCurrentIndex(index);

    // Load the stored image (through LocalImages)
    const QString imageName = p.value("image_path").toString();
    if (!imageName.isEmpty()) {
        m_imagePath = imageName;
        m_imageStored = true;

        if (m_images) {
            const int size = m_imageLabel->width();
            if (m_images->isCached(imageName, size)) {
                m_imageLabel->setPixmap(m_images->thumbnail(imageName, size));
            } else {
                m_images->requestPixmap(imageName, size);
            }
        }
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
    m_imageStored = false;
    QPixmap pixmap(filePath);
    m_imageLabel->setPixmap(pixmap.scaled(m_imageLabel->size(),
                                            Qt::KeepAspectRatio,
                                            Qt::SmoothTransformation));
}

void ProductDialog::onCostEditingFinished()
{
    QString raw = stripCurrency(m_costEdit->text()).trimmed();
    if (raw.isEmpty()) return;
    bool ok = false;
    double val = raw.toDouble(&ok);
    if (ok) {
        m_costEdit->setText(QString::number(val, 'f', AppConstants::PRICE_DECIMALS));
    }
}

void ProductDialog::onPriceEditingFinished()
{
    QString raw = stripCurrency(m_priceEdit->text()).trimmed();
    if (raw.isEmpty()) return;
    bool ok = false;
    double val = raw.toDouble(&ok);
    if (ok) {
        m_priceEdit->setText(QString::number(val, 'f', AppConstants::PRICE_DECIMALS));
    }
}

void ProductDialog::onHasVariantsChanged(bool checked)
{
    Q_UNUSED(checked)
    // Cost/Price stay visible but disabled (values kept, layout never jumps).
    updateStateUI();
}

void ProductDialog::onIsServiceChanged(bool checked)
{
    if (checked) {
        m_quantity->setValue(0);
        // A service has no stock: zero it out everywhere, like quantityEdit.
        for (PendingVariant& pv : m_pendingVariants)
            pv.stock = 0;
    }
    updateStateUI();
}

void ProductDialog::updateStateUI()
{
    const bool hasVariants = m_hasVariants->isChecked();
    const bool isService   = m_isService->isChecked();

    // Quantity slot: field <-> Manage variants pill <-> empty space
    const bool showStock = !hasVariants && !isService;
    if (m_stockLabel)
        m_stockLabel->setText(hasVariants ? tr("Variants") : tr("Quantity"));
    m_stockLabel->setVisible(showStock || hasVariants);
    m_quantity->setVisible(showStock);
    m_variantsBtn->setVisible(hasVariants);
    m_variantsBtn->setEnabled(hasVariants);

    m_costEdit->setEnabled(!hasVariants);
    m_priceEdit->setEnabled(!hasVariants);
}

void ProductDialog::updateMarginChip()
{
    bool costOk = false, priceOk = false;
    const double cost  = stripCurrency(m_costEdit->text()).toDouble(&costOk);
    const double price = stripCurrency(m_priceEdit->text()).toDouble(&priceOk);

    if (!costOk || !priceOk || price <= 0.0) {
        m_marginValue->setText("-");
        m_marginValue->setProperty("loss", false);
    } else {
        const double margin = (price - cost) / price * 100.0;
        const double delta  = price - cost;
        m_marginValue->setProperty("loss", delta < 0.0);
        m_marginValue->setText(QString("%1%  ·  %2%3")
                                   .arg(margin, 0, 'f', 1)
                                   .arg(delta >= 0 ? QStringLiteral("+") : QString())
                                   .arg(delta, 0, 'f', AppConstants::PRICE_DECIMALS));
    }
    m_marginValue->style()->unpolish(m_marginValue);
    m_marginValue->style()->polish(m_marginValue);
}

void ProductDialog::onVariantsAddBtnClicked()
{
    bool isService = m_isService->isChecked();
    VariantsDialog dialog(m_api, m_pendingVariants, m_editMode ? m_productId : -1, isService,
                          m_nameEdit->text().trimmed(), this);
    if (dialog.exec() == QDialog::Accepted) {
        m_pendingVariants = dialog.getPendingVariants();
        qDebug() << "ProductDialog: stored" << m_pendingVariants.size() << "pending variants";
    }
}

void ProductDialog::onAccepted()
{
    if (m_saving || !validate()) return;
    SaveJob job;
    job.edit = m_editMode;
    if (!(m_editMode ? buildUpdateJob(&job) : buildNewJob(&job))) return;
    startSave(job);
}

void ProductDialog::reject()
{
    if (m_saving) return;   // let the save finish; the window closes by itself
    ModernDialog::reject();
}

void ProductDialog::setSaving(bool saving)
{
    m_saving = saving;
    if (m_cancelBtn) m_cancelBtn->setEnabled(!saving);
    for (QWidget* w : {static_cast<QWidget*>(m_nameEdit), static_cast<QWidget*>(m_costEdit), static_cast<QWidget*>(m_priceEdit),
                       static_cast<QWidget*>(m_categorySelect), static_cast<QWidget*>(m_quantity), static_cast<QWidget*>(m_variantsBtn),
                       static_cast<QWidget*>(m_hasVariants), static_cast<QWidget*>(m_isService)})
        if (w) w->setEnabled(!saving);
    auto* spinner = static_cast<SaveSpinner*>(m_spinner);
    if (saving) {
        m_saveBtn->setMinimumWidth(m_saveBtn->width());
        m_saveBtn->setIcon(QIcon());
        m_saveBtn->setText(tr("     Saving…"));
        spinner->move(14, (m_saveBtn->height() - spinner->height()) / 2);
        spinner->start();
        setCursor(Qt::BusyCursor);
    } else {
        spinner->stop();
        m_saveBtn->setText(m_saveText);
        m_saveBtn->setIcon(LineIcons::icon("check", Qt::white, 16));
        setSaveStatus(QString());
        unsetCursor();
    }
}

void ProductDialog::setSaveStatus(const QString& text)
{
    m_saveStatus->setText(text);
    m_saveStatus->setVisible(!text.isEmpty());
}

void ProductDialog::startSave(const SaveJob& job)
{
    if (!m_api) {
        AppHelpers::showError(this, tr("The database is not available."));
        return;
    }
    setSaving(true);
    setSaveStatus(job.localImage.isEmpty() ? tr("Saving product…") : tr("Optimizing photo…"));

    // Turning a large photo upright and shrinking it takes a moment: it runs
    // on a worker thread so the window keeps moving. The database work stays
    // on this thread (its connection belongs here).
    QPointer<ProductDialog> self(this);
    auto result = std::make_shared<SaveResult>();
    const QString localImage = job.localImage;
    QThread* thread = QThread::create([localImage, result]() {
        if (localImage.isEmpty())
            return;
        QByteArray data;
        QString extension;
        if (shrinkPhoto(localImage, &data, &extension)) {
            // A unique name, so two photos called "image.jpg" never replace each other.
            result->imageName = "product_" + QUuid::createUuid().toString(QUuid::Id128).left(16) + "." + extension;
            result->imageData = data;
        } else {
            result->warning = QStringLiteral("unreadable");
        }
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    connect(thread, &QThread::finished, this, [this, self, result, job]() mutable {
        if (!self) return;
        if (result->warning == QLatin1String("unreadable"))
            result->warning = tr("The selected photo could not be read. The product was saved without it.");
        if (!result->imageName.isEmpty()) {
            setSaveStatus(tr("Saving photo…"));
            if (m_api->uploadImage(result->imageName, result->imageData)) {
                qDebug() << "Photo saved as" << result->imageName << "(" << result->imageData.size() / 1024 << "KB )";
                job.product["image_path"] = result->imageName;
            } else {
                result->warning = tr("The photo could not be saved (%1). The product was saved without it.")
                                      .arg(m_api->lastError());
                result->imageName.clear();
            }
        }
        setSaveStatus(tr("Saving product…"));
        result->id = m_api->saveProduct(job.product, job.variants, job.deleteIds);
        if (result->id < 0) {
            setSaving(false);
            AppHelpers::showError(this, (job.edit ? tr("Failed to update product: ") : tr("Failed to save product: "))
                                            + m_api->lastError());
            return;
        }
        // Show the new photo right away in this app.
        if (m_images && !result->imageName.isEmpty())
            m_images->remember(result->imageName, result->imageData);
        m_saving = false;
        if (!result->warning.isEmpty())
            AppHelpers::showWarning(this, result->warning);
        qDebug() << "Product saved, id:" << result->id;
        accept();
    });
    thread->start();
}

// ===== HELPERS =====

QString ProductDialog::stripCurrency(const QString& text) const
{
    QString result = text;
    result.remove(QString(" ") + AppConstants::CURRENCY, Qt::CaseInsensitive);
    return result.replace(',', '.').trimmed();
}

QVariant ProductDialog::imageValueFor(SaveJob* job) const
{
    if (m_imagePath.isEmpty()) return QVariant();
    if (m_imageStored) return m_imagePath;   // already stored
    job->localImage = m_imagePath;                // copied in by the save
    return QVariant();
}

// ===== VALIDATION & SAVE =====

bool ProductDialog::validate()
{
    if (m_nameEdit->text().trimmed().isEmpty()) {
        AppHelpers::showWarning(this, AppHelpers::trMsg(InventoryConstants::MSG_PRODUCT_NAME_EMPTY));
        return false;
    }

    bool hasVariants = m_hasVariants->isChecked();

    if (!hasVariants) {
        bool ok = false;
        stripCurrency(m_costEdit->text()).toDouble(&ok);
        if (!ok) {
            AppHelpers::showWarning(this, AppHelpers::trMsg(InventoryConstants::MSG_PRODUCT_COST_INVALID));
            return false;
        }

        stripCurrency(m_priceEdit->text()).toDouble(&ok);
        if (!ok) {
            AppHelpers::showWarning(this, AppHelpers::trMsg(InventoryConstants::MSG_PRODUCT_PRICE_INVALID));
            return false;
        }
    }

    return true;
}

bool ProductDialog::buildNewJob(SaveJob* job)
{
    bool isService   = m_isService->isChecked();
    bool hasVariants = m_hasVariants->isChecked();

    double cost  = hasVariants ? 0.0 : stripCurrency(m_costEdit->text()).toDouble();
    double price = hasVariants ? 0.0 : stripCurrency(m_priceEdit->text()).toDouble();
    int stock    = isService   ? 0   : m_quantity->value();
    int categoryId = m_categorySelect->currentData().toInt();

    const QVariant imageValue = imageValueFor(job);

    // product_save writes every product column, so all of them must be sent.
    const QVariantMap product{
        {"name", m_nameEdit->text().trimmed()},
        {"description", QVariant()},
        {"cost", cost},
        {"price", price},
        {"stock_quantity", stock},
        {"track_stock", !isService},
        {"min_stock_alert", 0},
        {"barcode", QVariant()},
        {"sku", QVariant()},
        {"category_id", categoryId > 0 ? QVariant(categoryId) : QVariant()},
        {"is_service", isService},
        {"has_variants", hasVariants},
        {"image_path", imageValue}
    };

    QList<QVariantMap> variants;
    if (hasVariants) {
        for (const PendingVariant& pv : m_pendingVariants) {
            variants.append(QVariantMap{
                {"variant_name", pv.name},
                {"cost", pv.cost},
                {"price", pv.price},
                {"stock", isService ? 0 : pv.stock},
                {"barcode", QVariant()},
                {"sku", QVariant()},
                {"is_active", 1}
            });
        }
    }

    job->product = product;
    job->variants = variants;
    return true;
}

bool ProductDialog::buildUpdateJob(SaveJob* job)
{
    bool isService   = m_isService->isChecked();
    bool hasVariants = m_hasVariants->isChecked();

    double cost  = hasVariants ? 0.0 : stripCurrency(m_costEdit->text()).toDouble();
    double price = hasVariants ? 0.0 : stripCurrency(m_priceEdit->text()).toDouble();
    int stock    = isService   ? 0   : m_quantity->value();
    int categoryId = m_categorySelect->currentData().toInt();

    const QVariant imageValue = imageValueFor(job);

    // product_save overwrites every product column: carry over the fields this
    // dialog does not edit (description/barcode/sku/min_stock_alert) so an
    // edit never wipes them.
    const QList<DataRow> existingRows =
        m_api->select("products", {{"id", m_productId}});
    QVariant description, barcode, sku;
    int minStockAlert = 0;
    if (!existingRows.isEmpty()) {
        const DataRow& e = existingRows.first();
        description    = e.value("description");
        barcode        = e.value("barcode");
        sku            = e.value("sku");
        minStockAlert  = e.value("min_stock_alert").toInt();
    }

    const QVariantMap product{
        {"id", m_productId},
        {"name", m_nameEdit->text().trimmed()},
        {"description", description},
        {"cost", cost},
        {"price", price},
        {"stock_quantity", stock},
        {"track_stock", !isService},
        {"min_stock_alert", minStockAlert},
        {"barcode", barcode},
        {"sku", sku},
        {"category_id", categoryId > 0 ? QVariant(categoryId) : QVariant()},
        {"is_service", isService},
        {"has_variants", hasVariants},
        {"image_path", imageValue}
    };

    // Sync variants only if the pending list was touched.
    QList<QVariantMap> variants;
    QList<int> deleteIds;
    if (hasVariants && !m_pendingVariants.isEmpty()) {
        QList<int> keptIds;
        for (const PendingVariant& pv : m_pendingVariants) {
            if (pv.variantId > 0) keptIds.append(pv.variantId);
        }

        // Variants removed in the dialog are deleted in the
        // same transaction.
        for (const DataRow& existing : m_api->variants(m_productId)) {
            const int existingId = existing.value("id").toInt();
            if (!keptIds.contains(existingId))
                deleteIds.append(existingId);
        }

        for (const PendingVariant& pv : m_pendingVariants) {
            QVariantMap v{
                {"variant_name", pv.name},
                {"cost", pv.cost},
                {"price", pv.price},
                {"stock", isService ? 0 : pv.stock},
                {"barcode", QVariant()},
                {"sku", QVariant()},
                {"is_active", 1}
            };
            if (pv.variantId > 0) v.insert("id", pv.variantId);
            variants.append(v);
        }
    }

    job->product = product;
    job->variants = variants;
    job->deleteIds = deleteIds;
    return true;
}
