#ifndef REGISTERCONSTANTS_H
#define REGISTERCONSTANTS_H

/**
 * Register-specific constants
 * For global constants, see Core/Prefixes.h
 */
namespace RegisterConstants {
// Product Display
constexpr int PRODUCT_BUTTON_SIZE    = 128;
constexpr int PRODUCT_IMAGE_SIZE     = 100;
constexpr int PRODUCT_GRID_SPACING = 5;
constexpr int PRODUCT_GRID_MARGIN = 10;
constexpr int MIN_COLUMNS = 1;
constexpr int DEFAULT_COLUMNS = 4;

// Cart Display
constexpr int CART_ITEM_HEIGHT = 64;
constexpr int CART_ITEM_SPACING = 6;
constexpr int CART_QUANTITY_SPINBOX_WIDTH = 60;
constexpr int CART_REMOVE_BUTTON_SIZE = 28;

// Cart Key Namespacing
// Product IDs are used directly as cart keys.
// Variant IDs are offset by this value to avoid collisions with product IDs.
constexpr int VARIANT_KEY_OFFSET = 1000000;
constexpr int LOW_STOCK_THRESHOLD = 3;
constexpr int DEFAULT_MAX_QUANTITY = 999;

// Messages - Cart
constexpr const char* MSG_CART_EMPTY = "Cart is empty!";
constexpr const char* MSG_CART_CLEAR_CONFIRM = "Clear all items from cart?";

// Messages - Stock
constexpr const char* MSG_OUT_OF_STOCK = "This product is out of stock.";
constexpr const char* MSG_INSUFFICIENT_STOCK = "Insufficient stock available.\nAvailable: %1, Required: %2";
constexpr const char* MSG_STOCK_EXCEEDED = "Cannot add more '%1'.\nOnly %2 items available.";

// Messages - Transaction
constexpr const char* MSG_SALE_COMPLETE = "Sale completed successfully!";
constexpr const char* MSG_CANCEL_CONFIRM = "Cancel current sale?";
constexpr const char* MSG_NO_USER_LOGGED_IN = "No user logged in. Cannot complete transaction.";
constexpr const char* MSG_TRANSACTION_FAILED = "Failed to save transaction. Please try again.";

// Messages - Stub Features (Coming Soon)
constexpr const char* MSG_TICKETS_COMING_SOON = "Tickets history dialog will be available soon.";
constexpr const char* MSG_NOTIFICATIONS_COMING_SOON = "Notifications feature will be available soon.";
constexpr const char* MSG_DISCOUNT_COMING_SOON = "Discount dialog will be available soon.";
constexpr const char* MSG_OPEN_TICKETS_COMING_SOON = "Open tickets dialog will be available soon.";
constexpr const char* MSG_CUSTOM_ITEM_COMING_SOON = "Custom item dialog will be available soon.";

// Default Images
constexpr const char* DEFAULT_PRODUCT_IMAGE = ":/Assets/images/Assets/product_default.png";
}

#endif // REGISTERCONSTANTS_H
