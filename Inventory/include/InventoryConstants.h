#ifndef INVENTORYCONSTANTS_H
#define INVENTORYCONSTANTS_H

#include <QCoreApplication>

/**
 * Inventory-specific constants
 * For global constants, see Core/Prefixes.h
 */
namespace InventoryConstants {

// ============================================================================
// PRODUCTS TABLE COLUMNS
// ============================================================================
// Column 0 (ID) is kept hidden — used internally for CRUD operations.
// Column 1 is the colored initial badge; column 2 is the product name.
// The remaining columns show category, stock, cost, price, and margin.
constexpr int COL_PRODUCT_ID       = 0;
constexpr int COL_PRODUCT_BADGE    = 1;
constexpr int COL_PRODUCT_NAME     = 2;
constexpr int COL_PRODUCT_CATEGORY = 3;
constexpr int COL_PRODUCT_STOCK    = 4;
constexpr int COL_PRODUCT_COST     = 5;
constexpr int COL_PRODUCT_PRICE    = 6;
constexpr int COL_PRODUCT_MARGIN   = 7;
constexpr int PRODUCT_COL_COUNT    = 8;

// ============================================================================
// CATEGORIES TABLE COLUMNS
// ============================================================================
constexpr int COL_CATEGORY_NAME        = 0;
constexpr int COL_CATEGORY_DESCRIPTION = 1;
constexpr int CATEGORY_COL_COUNT       = 2;

// ============================================================================
// DISCOUNTS TABLE COLUMNS
// ============================================================================
constexpr int COL_DISCOUNT_NAME     = 0;
constexpr int COL_DISCOUNT_VALUE    = 1;
constexpr int COL_DISCOUNT_ACTIVE   = 2;
constexpr int COL_DISCOUNT_END_DATE = 3;
constexpr int DISCOUNT_COL_COUNT    = 4;

// ============================================================================
// TABLE SELECT INDEXES
// ============================================================================
constexpr int TABLE_PRODUCTS   = 0;
constexpr int TABLE_CATEGORIES = 1;
constexpr int TABLE_DISCOUNTS  = 2;

// ============================================================================
// STOCK THRESHOLDS & VISUALS
// ============================================================================

// A product with stock <= LOW_STOCK_THRESHOLD (and > 0) is considered low stock.
// This constant is also used by the Register app for the badge label.
constexpr int LOW_STOCK_THRESHOLD  = 3;
constexpr int OUT_OF_STOCK_VALUE   = 0;

// Flash interval for out-of-stock rows (milliseconds)
constexpr int STOCK_FLASH_INTERVAL_MS = 800;

// Text colors applied to the Name and Stock cells only.
// Yellow for low stock — chosen to be legible on both dark and light themes.
// Red for out of stock — same rationale.
constexpr const char* COLOR_LOW_STOCK     = "#C8A000";   // amber-yellow
constexpr const char* COLOR_OUT_OF_STOCK  = "#E03030";   // clear red

// Default text colors per theme — used as the "off" state during flash.
// These match the QSS values already set in app_styles_dark.qss / app_styles_light.qss.
constexpr const char* COLOR_TEXT_DARK_THEME  = "#E6EAF0";
constexpr const char* COLOR_TEXT_LIGHT_THEME = "#40331F";

// ============================================================================
// MESSAGES - GENERAL
// ============================================================================
constexpr const char* MSG_NO_PERMISSION     = QT_TRANSLATE_NOOP("Messages", "You don't have permission to perform this action.");

// ============================================================================
// MESSAGES - PRODUCTS
// ============================================================================
constexpr const char* MSG_PRODUCT_ADD_SUCCESS    = QT_TRANSLATE_NOOP("Messages", "Product added successfully.");
constexpr const char* MSG_PRODUCT_EDIT_SUCCESS   = QT_TRANSLATE_NOOP("Messages", "Product updated successfully.");
constexpr const char* MSG_PRODUCT_DELETE_CONFIRM = QT_TRANSLATE_NOOP("Messages", "Are you sure you want to delete this product?");
constexpr const char* MSG_PRODUCT_DELETE_SUCCESS = QT_TRANSLATE_NOOP("Messages", "Product deleted successfully.");
constexpr const char* MSG_PRODUCT_NAME_EMPTY     = QT_TRANSLATE_NOOP("Messages", "Product name cannot be empty.");
constexpr const char* MSG_PRODUCT_PRICE_INVALID  = QT_TRANSLATE_NOOP("Messages", "Please enter a valid price.");
constexpr const char* MSG_PRODUCT_COST_INVALID   = QT_TRANSLATE_NOOP("Messages", "Please enter a valid cost.");
constexpr const char* MSG_PRODUCT_STOCK_INVALID  = QT_TRANSLATE_NOOP("Messages", "Please enter a valid stock quantity.");

// ============================================================================
// MESSAGES - CATEGORIES
// ============================================================================
constexpr const char* MSG_CATEGORY_ADD_SUCCESS    = QT_TRANSLATE_NOOP("Messages", "Category added successfully.");
constexpr const char* MSG_CATEGORY_EDIT_SUCCESS   = QT_TRANSLATE_NOOP("Messages", "Category updated successfully.");
constexpr const char* MSG_CATEGORY_DELETE_SUCCESS = QT_TRANSLATE_NOOP("Messages", "Category deleted successfully.");
constexpr const char* MSG_CATEGORY_NAME_EMPTY     = QT_TRANSLATE_NOOP("Messages", "Category name cannot be empty.");

// ============================================================================
// MESSAGES - DISCOUNTS
// ============================================================================
constexpr const char* MSG_DISCOUNT_ADD_SUCCESS    = QT_TRANSLATE_NOOP("Messages", "Discount added successfully.");
constexpr const char* MSG_DISCOUNT_EDIT_SUCCESS   = QT_TRANSLATE_NOOP("Messages", "Discount updated successfully.");
constexpr const char* MSG_DISCOUNT_DELETE_CONFIRM = QT_TRANSLATE_NOOP("Messages", "Are you sure you want to delete this discount?");
constexpr const char* MSG_DISCOUNT_DELETE_SUCCESS = QT_TRANSLATE_NOOP("Messages", "Discount deleted successfully.");
constexpr const char* MSG_DISCOUNT_NAME_EMPTY     = QT_TRANSLATE_NOOP("Messages", "Discount name cannot be empty.");

// ============================================================================
// MESSAGES - VARIANTS
// ============================================================================
constexpr const char* MSG_VARIANT_NAME_EMPTY    = QT_TRANSLATE_NOOP("Messages", "Variant name cannot be empty.");
constexpr const char* MSG_VARIANT_PRICE_INVALID = QT_TRANSLATE_NOOP("Messages", "Please enter a valid price for all variants.");
constexpr const char* MSG_VARIANT_COST_INVALID  = QT_TRANSLATE_NOOP("Messages", "Please enter a valid cost for all variants.");
constexpr const char* MSG_VARIANT_NO_VARIANTS   = QT_TRANSLATE_NOOP("Messages", "Please add at least one variant.");

// ============================================================================
// MESSAGES - STOCK
// ============================================================================
constexpr const char* MSG_STOCK_INVALID = QT_TRANSLATE_NOOP("Messages", "Please enter a valid stock quantity.");

// ============================================================================
// MESSAGES - LOGIN
// ============================================================================
constexpr const char* MSG_ACCESS_DENIED = QT_TRANSLATE_NOOP("Messages", "Access denied. Only Admins and Managers can access the Inventory.");

// ============================================================================
// IMAGE
// ============================================================================
constexpr const char* IMAGE_PLACEHOLDER_TEXT = "Click to select image";

} // namespace InventoryConstants

#endif // INVENTORYCONSTANTS_H
