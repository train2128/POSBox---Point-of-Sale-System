#ifndef PRODUCTBUTTONCREATOR_H
#define PRODUCTBUTTONCREATOR_H

#include "ProductButton.h"
#include <QHash>
#include <QVariant>

class DatabaseManager;

/**
 * Static factory for creating product buttons.
 * Returns a ProductButton (QFrame subclass) ready to add to the grid.
 */
class ProductButtonCreator
{
public:
    static ProductButton* createButton(const QHash<QString, QVariant>& product,
                                       DatabaseManager* db = nullptr);

private:
    ProductButtonCreator() = delete;
    ~ProductButtonCreator() = delete;
    ProductButtonCreator(const ProductButtonCreator&) = delete;
    ProductButtonCreator& operator=(const ProductButtonCreator&) = delete;
};

#endif // PRODUCTBUTTONCREATOR_H
