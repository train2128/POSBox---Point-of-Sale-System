#ifndef PRODUCTBUTTONCREATOR_H
#define PRODUCTBUTTONCREATOR_H

#include "ProductButton.h"
#include <QHash>
#include <QVariant>
#include <QPixmap>

class LocalImages;

/**
 * Static factory for creating product buttons.
 * Returns a ProductButton (QFrame subclass) ready to add to the grid.
 */
class ProductButtonCreator
{
public:
    static ProductButton* createButton(const QHash<QString, QVariant>& product,
                                       LocalImages* images = nullptr);

    /**
     * Same as createButton() but without downloading the product image:
     * the button starts with the default art and the caller applies the
     * real image later via applyImage(). Keeps grid creation instant when
     * many images would otherwise be downloaded synchronously.
     */
    static ProductButton* createButtonDeferred(const QHash<QString, QVariant>& product);

    /**
     * Download (RAM-cached by LocalImages) and display the product image
     * on a button previously created with createButtonDeferred(). No-op
     * when the product has no image or the download fails (default art
     * stays visible).
     */
    static void applyImage(ProductButton* button, LocalImages* images);

    /**
     * Apply an already-available pixmap (e.g. delivered asynchronously by
     * LocalImages::pixmapReady) to a button. No-op for null pixmaps.
     */
    static void applyPixmap(ProductButton* button, const QPixmap& pixmap);

private:
    static ProductButton* createButtonInternal(const QHash<QString, QVariant>& product);

    ProductButtonCreator() = delete;
    ~ProductButtonCreator() = delete;
    ProductButtonCreator(const ProductButtonCreator&) = delete;
    ProductButtonCreator& operator=(const ProductButtonCreator&) = delete;
};

#endif // PRODUCTBUTTONCREATOR_H
