#include "ProductButtonCreator.h"
#include "ProductButton.h"
#include "RegisterConstants.h"
#include "Prefixes.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QFile>

ProductButton* ProductButtonCreator::createButton(const QHash<QString, QVariant>& product,
                                                  DatabaseManager* db)
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
    button->setFixedSize(RegisterConstants::PRODUCT_BUTTON_SIZE,
                         RegisterConstants::PRODUCT_BUTTON_SIZE);
    button->setEnabled(isService || stock > 0);
    button->setToolTip(name);

    // Store product data
    button->setProperty("productId",    productId);
    button->setProperty("productName",  name);
    button->setProperty("productPrice", price);
    button->setProperty("productStock", stock);
    button->setProperty("isService",    isService);
    button->setProperty("hasVariants",  hasVariants);
    button->setProperty("categoryId",   categoryId);
    button->setProperty("categoryName", categoryName);

    // ── Inner layout ──────────────────────────────────────────────
    QVBoxLayout* layout = new QVBoxLayout(button);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(2);

    // Stock / service badge
    QLabel* stockLabel = new QLabel(button);
    stockLabel->setAlignment(Qt::AlignTop | Qt::AlignRight);

    if (isService) {
        stockLabel->setText("Service");
        stockLabel->setObjectName("product-stock-service");
    } else if (stock > RegisterConstants::LOW_STOCK_THRESHOLD) {
        stockLabel->setText(QString("%1 left").arg(stock));
        stockLabel->setObjectName("product-stock-ok");
    } else {
        stockLabel->setText(QString("%1 left").arg(stock));
        stockLabel->setObjectName("product-stock-low");
    }

    // Product image
    QLabel* imageLabel = new QLabel(button);
    imageLabel->setObjectName("product-image");
    imageLabel->setAlignment(Qt::AlignCenter);

    QPixmap pixmap;
    if (!imagePath.isEmpty() && QFile::exists(imagePath))
        pixmap.load(imagePath);
    else
        pixmap.load(RegisterConstants::DEFAULT_PRODUCT_IMAGE);

    imageLabel->setPixmap(pixmap.scaled(RegisterConstants::PRODUCT_IMAGE_SIZE,
                                        RegisterConstants::PRODUCT_IMAGE_SIZE,
                                        Qt::KeepAspectRatio,
                                        Qt::SmoothTransformation));

    // Price label (non-variant products only)
    QLabel* priceLabel = nullptr;
    if (!hasVariants) {
        priceLabel = new QLabel(AppHelpers::formatPrice(price), button);
        priceLabel->setObjectName("product-price-label");
        priceLabel->setAlignment(Qt::AlignBottom | Qt::AlignCenter);
    }

    layout->addWidget(stockLabel);
    layout->addWidget(imageLabel, 1);
    if (priceLabel)
        layout->addWidget(priceLabel);

    return button;
}
