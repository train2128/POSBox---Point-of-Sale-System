#ifndef REGISTERCONSTANTS_H
#define REGISTERCONSTANTS_H

#include <QCoreApplication>

/**
 * Register-specific constants
 * For global constants, see Core/Prefixes.h
 */
namespace RegisterConstants {
// Product Display
constexpr int PRODUCT_BUTTON_SIZE    = 128;
constexpr int PRODUCT_IMAGE_SIZE     = 160;  // size requested from the image cache
constexpr int PRODUCT_IMAGE_SHOWN    = 80;
constexpr int PRODUCT_IMAGE_AREA     = 88;
constexpr int PRODUCT_IMAGE_WIDTH    = 114;  // the tile, less its border and margins
constexpr int PRODUCT_GRID_SPACING = 8;
constexpr int PRODUCT_GRID_MARGIN = 10;
constexpr int MIN_COLUMNS = 1;
constexpr int DEFAULT_COLUMNS = 4;

// Cart Display
constexpr int CART_ITEM_SPACING = 6;
constexpr int CART_QUANTITY_SPINBOX_WIDTH = 34;
constexpr int CART_STEP_BUTTON_SIZE = 26;
constexpr int CART_REMOVE_BUTTON_SIZE = 24;

// Cart Key Namespacing
// Product IDs are used directly as cart keys.
// Variant IDs are offset by this value to avoid collisions with product IDs.
constexpr int VARIANT_KEY_OFFSET = 1000000;
constexpr int LOW_STOCK_THRESHOLD = 3;
constexpr int DEFAULT_MAX_QUANTITY = 999;

// Messages - Cart
constexpr const char* MSG_CART_EMPTY = QT_TRANSLATE_NOOP("Messages", "Cart is empty!");

// Messages - Stock
constexpr const char* MSG_STOCK_EXCEEDED = QT_TRANSLATE_NOOP("Messages", "Cannot add more '%1'.\nOnly %2 items available.");

// Messages - Transaction
constexpr const char* MSG_CANCEL_CONFIRM = QT_TRANSLATE_NOOP("Messages", "Cancel current sale?");
constexpr const char* MSG_NO_USER_LOGGED_IN = QT_TRANSLATE_NOOP("Messages", "No user logged in. Cannot complete transaction.");
constexpr const char* MSG_TRANSACTION_FAILED = QT_TRANSLATE_NOOP("Messages", "Failed to save transaction. Please try again.");

// Messages - Stub Features (Coming Soon)

// Default Images
constexpr const char* DEFAULT_PRODUCT_IMAGE = ":/Assets/images/Assets/product_default.png";
}

#endif // REGISTERCONSTANTS_H
