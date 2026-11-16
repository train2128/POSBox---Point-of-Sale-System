#include <QCoreApplication>
#include "ProductButtonCreator.h"
#include "Animations.h"
#include "ProductButton.h"
#include "RegisterConstants.h"
#include "Prefixes.h"
#include "LocalImages.h"
#include "ElidedLabel.h"
#include <QGridLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QApplication>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QFile>

namespace {

bool hasTransparency(const QImage& image)
{
    if (!image.hasAlphaChannel())
        return false;
    const QImage pixels = image.convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < pixels.height(); ++y) {
        const QRgb* line = reinterpret_cast<const QRgb*>(pixels.constScanLine(y));
        for (int x = 0; x < pixels.width(); ++x)
            if (qAlpha(line[x]) < 200)
                return true;
    }
    return false;
}

// The picture for a tile's image area. A photo fills it, cropped evenly and with
// rounded corners; a picture with a transparent background is shown whole, so
// the tile shows through behind it.
QPixmap tilePicture(const QPixmap& source)
{
    if (source.isNull())
        return source;

    const qreal ratio = qApp->devicePixelRatio();
    const QSize area(qRound(RegisterConstants::PRODUCT_IMAGE_WIDTH * ratio),
                     qRound(RegisterConstants::PRODUCT_IMAGE_AREA * ratio));
    const QImage image = source.toImage();

    QPixmap picture;
    if (hasTransparency(image)) {
        const int side = qRound(RegisterConstants::PRODUCT_IMAGE_SHOWN * ratio);
        picture = source.scaled(side, side, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    } else {
        const QImage scaled = image.scaled(area, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        const QImage cropped = scaled.copy((scaled.width() - area.width()) / 2,
                                           (scaled.height() - area.height()) / 2,
                                           area.width(), area.height());
        QImage rounded(area, QImage::Format_ARGB32_Premultiplied);
        rounded.fill(Qt::transparent);
        QPainter painter(&rounded);
        painter.setRenderHint(QPainter::Antialiasing);
        QPainterPath corners;
        corners.addRoundedRect(QRectF(rounded.rect()), 8 * ratio, 8 * ratio);
        painter.setClipPath(corners);
        painter.drawImage(0, 0, cropped);
        painter.end();
        picture = QPixmap::fromImage(rounded);
    }
    picture.setDevicePixelRatio(ratio);
    return picture;
}

} // namespace

ProductButton* ProductButtonCreator::createButton(const QHash<QString, QVariant>& product,
                                                  LocalImages* images)
{
    ProductButton* button = createButtonInternal(product);
    applyImage(button, images);
    return button;
}

ProductButton* ProductButtonCreator::createButtonDeferred(const QHash<QString, QVariant>& product)
{
    return createButtonInternal(product);
}

void ProductButtonCreator::applyImage(ProductButton* button, LocalImages* images)
{
    if (!button || !images)
        return;

    QString imagePath = button->property("imagePath").toString();
    if (imagePath.isEmpty())
        return;

    applyPixmap(button, images->pixmap(imagePath));
}

void ProductButtonCreator::applyPixmap(ProductButton* button, const QPixmap& pixmap)
{
    if (!button || pixmap.isNull())
        return;

    QLabel* imageLabel = button->findChild<QLabel*>("product-image");
    if (!imageLabel)
        return;

    imageLabel->setPixmap(tilePicture(pixmap));
}

ProductButton* ProductButtonCreator::createButtonInternal(const QHash<QString, QVariant>& product)
{
    // Extract product data
    int     productId   = product.value("id").toInt();
    QString name        = product.value("name").toString();
    double  price       = product.value("price").toDouble();
    int     stock       = product.value("stock").toInt();
    QString imagePath   = product.value("image_path").toString();
    bool    isService   = product.value("is_service").toBool();
    int     categoryId  = product.value("category_id").toInt();
    QString categoryName = product.value("category").toString();

    bool    hasVariants  = product.value("has_variants").toBool();

    // ── Button frame ──────────────────────────────────────────────
    ProductButton* button = new ProductButton();
    button->setFixedWidth(RegisterConstants::PRODUCT_BUTTON_SIZE);
    button->setEnabled(isService || stock > 0);
    button->setToolTip(name);
    Animations::enableGlare(button);

    // Store product data
    button->setProperty("productId",    productId);
    button->setProperty("productName",  name);
    button->setProperty("productPrice", price);
    button->setProperty("productStock", stock);
    button->setProperty("isService",    isService);
    button->setProperty("hasVariants",  hasVariants);
    button->setProperty("categoryId",   categoryId);
    button->setProperty("categoryName", categoryName);
    button->setProperty("imagePath",    imagePath);

    // ── Inner layout ──────────────────────────────────────────────
    QVBoxLayout* layout = new QVBoxLayout(button);
    layout->setContentsMargins(6, 6, 6, 7);
    layout->setSpacing(2);

    // Product image — default art first. createButton()/applyImage()
    // replaces it with the product picture when one is available.
    QLabel* imageLabel = new QLabel(button);
    imageLabel->setObjectName("product-image");
    imageLabel->setAlignment(Qt::AlignCenter);
    imageLabel->setFixedSize(RegisterConstants::PRODUCT_IMAGE_WIDTH, RegisterConstants::PRODUCT_IMAGE_AREA);

    QPixmap pixmap;
    pixmap.load(RegisterConstants::DEFAULT_PRODUCT_IMAGE);

    imageLabel->setPixmap(tilePicture(pixmap));

    // Stock / service badge, in the image's top right corner
    QLabel* stockLabel = new QLabel(button);
    if (isService) {
        stockLabel->setText(QCoreApplication::translate("RegisterWindow", "Service"));
        stockLabel->setObjectName("product-stock-service");
    } else if (stock <= 0) {
        stockLabel->setText(QCoreApplication::translate("RegisterWindow", "Out of stock"));
        stockLabel->setObjectName("product-stock-out");
    } else if (stock > RegisterConstants::LOW_STOCK_THRESHOLD) {
        stockLabel->setText(QCoreApplication::translate("RegisterWindow", "%1 left").arg(stock));
        stockLabel->setObjectName("product-stock-ok");
    } else {
        stockLabel->setText(QCoreApplication::translate("RegisterWindow", "%1 left").arg(stock));
        stockLabel->setObjectName("product-stock-low");
    }

    QGridLayout* imageArea = new QGridLayout;
    imageArea->setContentsMargins(0, 0, 0, 0);
    imageArea->addWidget(imageLabel, 0, 0);
    imageArea->addWidget(stockLabel, 0, 0, Qt::AlignTop | Qt::AlignRight);

    // Name, and the price (products with variants price each variant)
    ElidedLabel* nameLabel = new ElidedLabel(name, button);
    nameLabel->setObjectName("product-name");

    QLabel* priceLabel = new QLabel(
        hasVariants ? QCoreApplication::translate("RegisterWindow", "Choose option")
                    : QString("%1 %2").arg(AppHelpers::formatAmount(price), AppConstants::CURRENCY),
        button);
    priceLabel->setObjectName("product-price-label");

    layout->addLayout(imageArea);
    layout->addSpacing(3);
    layout->addWidget(nameLabel);
    layout->addWidget(priceLabel);

    return button;
}
