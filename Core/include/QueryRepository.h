#ifndef QUERYREPOSITORY_H
#define QUERYREPOSITORY_H

#include <QString>

namespace Queries {

// ============================================================================
// ACCOUNTS
// ============================================================================
const QString ACCOUNT_INSERT =
    "INSERT INTO accounts (username, password_hash, role, display_name, email, phone) "
    "VALUES (?, ?, ?, ?, ?, ?)";

const QString ACCOUNT_UPDATE =
    "UPDATE accounts SET password_hash = ?, role = ?, display_name = ?, email = ?, phone = ?, is_active = ? "
    "WHERE username = ?";

const QString ACCOUNT_DELETE =
    "DELETE FROM accounts WHERE username = ?";

const QString ACCOUNT_SELECT_BY_USERNAME =
    "SELECT username, password_hash, role, display_name, email, phone, is_active, last_login "
    "FROM accounts WHERE username = ?";

const QString ACCOUNT_SELECT_ALL =
    "SELECT username, password_hash, role, display_name, email, phone, is_active, created_at, last_login "
    "FROM accounts ORDER BY username";

const QString ACCOUNT_UPDATE_LAST_LOGIN =
    "UPDATE accounts SET last_login = CURRENT_TIMESTAMP WHERE username = ?";

// ============================================================================
// CATEGORIES
// ============================================================================
const QString CATEGORY_INSERT =
    "INSERT INTO categories (name, description, display_order, color, icon_path, visible_in_retail, visible_in_cafe) "
    "VALUES (?, ?, ?, ?, ?, ?, ?)";

const QString CATEGORY_UPDATE =
    "UPDATE categories SET name = ?, description = ?, display_order = ?, color = ?, icon_path = ?, "
    "visible_in_retail = ?, visible_in_cafe = ? WHERE id = ?";

const QString CATEGORY_DELETE =
    "DELETE FROM categories WHERE id = ?";

const QString CATEGORY_SELECT_ALL =
    "SELECT id, name, description, display_order, color, icon_path, visible_in_retail, visible_in_cafe "
    "FROM categories ORDER BY display_order, name";

const QString CATEGORY_SELECT_BY_ID =
    "SELECT id, name, description, display_order, color, icon_path, visible_in_retail, visible_in_cafe "
    "FROM categories WHERE id = ?";

// ============================================================================
// PRODUCTS
// ============================================================================
const QString PRODUCT_INSERT =
    "INSERT INTO products (name, description, cost, price, stock_quantity, track_stock, min_stock_alert, "
    "barcode, sku, category_id, is_service, has_variants, image_path) "
    "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

const QString PRODUCT_UPDATE =
    "UPDATE products SET name = ?, description = ?, cost = ?, price = ?, stock_quantity = ?, track_stock = ?, "
    "min_stock_alert = ?, barcode = ?, sku = ?, category_id = ?, is_service = ?, has_variants = ?, image_path = ?, "
    "updated_at = CURRENT_TIMESTAMP WHERE id = ?";

const QString PRODUCT_DELETE =
    "DELETE FROM products WHERE id = ?";

const QString PRODUCT_SELECT_ALL =
    "SELECT p.id, p.name, p.description, p.cost, p.price, p.stock_quantity, p.track_stock, p.min_stock_alert, "
    "p.barcode, p.sku, p.category_id, c.name as category_name, p.is_service, p.has_variants, p.image_path, "
    "p.created_at, p.updated_at, "
    "COALESCE("
    "    CASE WHEN p.has_variants = 1 THEN "
    "        (SELECT SUM(stock) FROM product_variants WHERE parent_product_id = p.id AND is_active = 1) "
    "    ELSE p.stock_quantity END, 0"
    ") as stock "
    "FROM products p LEFT JOIN categories c ON p.category_id = c.id "
    "ORDER BY p.name";

const QString PRODUCT_SELECT_BY_ID =
    "SELECT p.id, p.name, p.description, p.cost, p.price, p.stock_quantity, p.track_stock, p.min_stock_alert, "
    "p.barcode, p.sku, p.category_id, c.name as category_name, p.is_service, p.has_variants, p.image_path, "
    "COALESCE("
    "    CASE WHEN p.has_variants = 1 THEN "
    "        (SELECT SUM(stock) FROM product_variants WHERE parent_product_id = p.id AND is_active = 1) "
    "    ELSE p.stock_quantity END, 0"
    ") as stock "
    "FROM products p LEFT JOIN categories c ON p.category_id = c.id WHERE p.id = ?";

const QString PRODUCT_SELECT_BY_BARCODE =
    "SELECT id, name, cost, price, stock_quantity FROM products WHERE barcode = ?";

const QString PRODUCT_UPDATE_STOCK =
    "UPDATE products SET stock_quantity = stock_quantity + ? WHERE id = ?";

const QString OUT_OF_STOCK_SET =
    "UPDATE products SET out_of_stock_since = CURRENT_TIMESTAMP WHERE id = ? AND stock_quantity = 0";

const QString OUT_OF_STOCK_CLEAR =
    "UPDATE products SET out_of_stock_since = NULL WHERE id = ? AND stock_quantity > 0";

const QString PRODUCT_SELECT_OUT_OF_STOCK_7_DAYS =
    "SELECT id, name, out_of_stock_since FROM products WHERE out_of_stock_since IS NOT NULL AND out_of_stock_since < datetime('now', '-7 days')";

const QString OUT_OF_STOCK_VARIANT_SET =
    "UPDATE product_variants SET out_of_stock_since = CURRENT_TIMESTAMP WHERE id = ? AND stock = 0";

const QString OUT_OF_STOCK_VARIANT_CLEAR =
    "UPDATE product_variants SET out_of_stock_since = NULL WHERE id = ? AND stock > 0";

const QString VARIANT_ALL_OUT_OF_STOCK_SET =
    "UPDATE products SET out_of_stock_since = CURRENT_TIMESTAMP WHERE id = ? "
    "AND has_variants = 1 AND id NOT IN "
    "(SELECT parent_product_id FROM product_variants WHERE parent_product_id = ? AND is_active = 1 AND stock > 0)";

const QString VARIANT_ANY_IN_STOCK_CLEAR =
    "UPDATE products SET out_of_stock_since = NULL WHERE id = ? "
    "AND EXISTS (SELECT 1 FROM product_variants WHERE parent_product_id = ? AND is_active = 1 AND stock > 0)";

const QString VARIANT_UPDATE_STOCK =
    "UPDATE product_variants SET stock = stock + ? WHERE id = ?";

// ============================================================================
// PRODUCT VARIANTS
// ============================================================================
const QString VARIANT_INSERT =
    "INSERT INTO product_variants (parent_product_id, variant_name, cost, price, stock, barcode, sku) "
    "VALUES (?, ?, ?, ?, ?, ?, ?)";

const QString VARIANT_UPDATE =
    "UPDATE product_variants SET variant_name = ?, cost = ?, price = ?, stock = ?, barcode = ?, sku = ?, is_active = ? "
    "WHERE id = ?";

const QString VARIANT_DELETE =
    "DELETE FROM product_variants WHERE id = ?";

const QString VARIANT_SELECT_BY_PRODUCT =
    "SELECT id, variant_name, cost, price, stock, barcode, sku, is_active "
    "FROM product_variants WHERE parent_product_id = ? AND is_active = 1 ORDER BY variant_name";

const QString VARIANT_SELECT_BY_BARCODE =
    "SELECT id, parent_product_id, variant_name, cost, price, stock FROM product_variants WHERE barcode = ?";

// ============================================================================
// TICKETS
// ============================================================================
const QString TICKET_INSERT =
    "INSERT INTO tickets (ticket_number, date, time, seller_username, seller_display_name, "
    "subtotal, discount_amount, tax_amount, total, payment_method, status, "
    "customer_name, customer_phone, deposit_amount, description, due_date) "
    "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

const QString TICKET_UPDATE_STATUS =
    "UPDATE tickets SET status = ?, date = ? WHERE id = ?";

const QString TICKET_DELETE =
    "DELETE FROM tickets WHERE id = ?";

const QString TICKET_SELECT_ALL =
    "SELECT id, ticket_number, date, time, seller_username, seller_display_name, "
    "subtotal, discount_amount, tax_amount, total, payment_method, status, "
    "customer_name, customer_phone, deposit_amount, description, due_date, created_at "
    "FROM tickets ORDER BY date DESC, time DESC";

const QString TICKET_SELECT_BY_ID =
    "SELECT id, ticket_number, date, time, seller_username, seller_display_name, "
    "subtotal, discount_amount, tax_amount, total, payment_method, status, "
    "customer_name, customer_phone, deposit_amount, description, due_date "
    "FROM tickets WHERE id = ?";

const QString TICKET_SELECT_BY_DATE =
    "SELECT id, ticket_number, date, time, seller_display_name, total, discount_amount, status "
    "FROM tickets WHERE date = ? AND status != 'OPEN' ORDER BY time DESC";

const QString TICKET_SELECT_OPEN =
    "SELECT id, ticket_number, customer_name, total, deposit_amount, due_date "
    "FROM tickets WHERE status = 'OPEN' ORDER BY due_date";

// ============================================================================
// TICKET ITEMS
// ============================================================================
const QString TICKET_ITEM_INSERT =
    "INSERT INTO ticket_items (ticket_id, product_id, variant_id, product_name, variant_name, "
    "category_name, barcode, quantity, unit_price, unit_cost, total_price) "
    "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

const QString TICKET_ITEM_SELECT_BY_TICKET =
    "SELECT id, product_id, variant_id, product_name, variant_name, category_name, barcode, "
    "quantity, unit_price, unit_cost, total_price "
    "FROM ticket_items WHERE ticket_id = ?";

const QString TICKET_ITEM_DELETE_BY_TICKET =
    "DELETE FROM ticket_items WHERE ticket_id = ?";

// ============================================================================
// DISCOUNTS
// ============================================================================
const QString DISCOUNT_INSERT =
    "INSERT INTO discounts (name, type, value, is_active, start_date, end_date) "
    "VALUES (?, ?, ?, ?, ?, ?)";

const QString DISCOUNT_UPDATE =
    "UPDATE discounts SET name = ?, type = ?, value = ?, is_active = ?, start_date = ?, end_date = ? "
    "WHERE id = ?";

const QString DISCOUNT_DELETE =
    "DELETE FROM discounts WHERE id = ?";

const QString DISCOUNT_SELECT_ALL =
    "SELECT id, name, type, value, is_active, start_date, end_date "
    "FROM discounts ORDER BY name";

const QString DISCOUNT_SELECT_ACTIVE =
    "SELECT id, name, type, value FROM discounts WHERE is_active = 1 "
    "AND (start_date IS NULL OR start_date <= date('now')) "
    "AND (end_date IS NULL OR end_date >= date('now'))";

// ============================================================================
// RECURRING EXPENSES
// ============================================================================
const QString EXPENSE_INSERT =
    "INSERT INTO recurring_expenses (name, is_active, type, amount, recurrence_unit, recurrence_value, "
    "start_date, next_due_date, notes, category) "
    "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

const QString EXPENSE_UPDATE =
    "UPDATE recurring_expenses SET name = ?, is_active = ?, type = ?, amount = ?, recurrence_unit = ?, "
    "recurrence_value = ?, start_date = ?, next_due_date = ?, notes = ?, category = ?, "
    "last_modified = CURRENT_TIMESTAMP WHERE id = ?";

const QString EXPENSE_DELETE =
    "DELETE FROM recurring_expenses WHERE id = ?";

const QString EXPENSE_SELECT_ALL =
    "SELECT id, name, is_active, type, amount, recurrence_unit, recurrence_value, "
    "start_date, next_due_date, notes, category, last_modified "
    "FROM recurring_expenses ORDER BY next_due_date";

const QString EXPENSE_SELECT_DUE =
    "SELECT id, name, amount, next_due_date FROM recurring_expenses "
    "WHERE is_active = 1 AND next_due_date <= date('now', '+7 days')";

// ============================================================================
// ONE-TIME EXPENSES
// ============================================================================
const QString ONE_TIME_EXPENSE_INSERT =
    "INSERT INTO one_time_expenses (name, amount, date, notes) "
    "VALUES (?, ?, ?, ?)";

const QString ONE_TIME_EXPENSE_DELETE =
    "DELETE FROM one_time_expenses WHERE id = ?";

const QString ONE_TIME_EXPENSE_SELECT_BY_MONTH =
    "SELECT id, name, amount, date, notes "
    "FROM one_time_expenses "
    "WHERE strftime('%Y', date) = ? AND strftime('%m', date) = ? "
    "ORDER BY date DESC";

const QString ONE_TIME_EXPENSE_SELECT_BY_YEAR =
    "SELECT id, name, amount, date, notes "
    "FROM one_time_expenses "
    "WHERE strftime('%Y', date) = ? "
    "ORDER BY date DESC";

// ============================================================================
// RECURRING EXPENSE PAYMENTS
// ============================================================================
const QString REC_EXPENSE_PAYMENT_INSERT =
    "INSERT INTO recurring_expense_payments (expense_id, amount, paid_date) "
    "VALUES (?, ?, ?)";

const QString REC_EXPENSE_PAYMENT_SELECT_BY_MONTH =
    "SELECT p.id, p.expense_id, p.amount, p.paid_date, e.name "
    "FROM recurring_expense_payments p "
    "JOIN recurring_expenses e ON p.expense_id = e.id "
    "WHERE strftime('%Y', p.paid_date) = ? AND strftime('%m', p.paid_date) = ? "
    "ORDER BY p.paid_date DESC";

const QString REC_EXPENSE_PAYMENT_SELECT_BY_YEAR =
    "SELECT p.id, p.expense_id, p.amount, p.paid_date, e.name "
    "FROM recurring_expense_payments p "
    "JOIN recurring_expenses e ON p.expense_id = e.id "
    "WHERE strftime('%Y', p.paid_date) = ? "
    "ORDER BY p.paid_date DESC";

// ============================================================================
// SHOP PROFILE
// ============================================================================
const QString PROFILE_INSERT =
    "INSERT INTO shop_profile (id, shop_name, address, postal, city, country, phone, email, website, "
    "facebook, instagram, tiktok, whatsapp, business_mode, currency, tax_rate, logo_path) "
    "VALUES (1, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

const QString PROFILE_UPDATE =
    "UPDATE shop_profile SET shop_name = ?, address = ?, postal = ?, city = ?, country = ?, "
    "phone = ?, email = ?, website = ?, facebook = ?, instagram = ?, tiktok = ?, whatsapp = ?, "
    "business_mode = ?, currency = ?, tax_rate = ?, logo_path = ?, updated_at = CURRENT_TIMESTAMP "
    "WHERE id = 1";

const QString PROFILE_SELECT =
    "SELECT shop_name, address, postal, city, country, phone, email, website, "
    "facebook, instagram, tiktok, whatsapp, business_mode, currency, tax_rate, logo_path "
    "FROM shop_profile WHERE id = 1";

// ============================================================================
// APP SETTINGS
// ============================================================================
const QString APP_SETTINGS_SELECT =
    "SELECT color_scheme, font_size, background, show_oos_products, sync_path, last_backup, "
    "country, currency, language FROM app_settings WHERE id = 1";

const QString APP_SETTINGS_UPDATE_CURRENCY =
    "UPDATE app_settings SET country = ?, currency = ? WHERE id = 1";

const QString APP_SETTINGS_UPDATE_LANGUAGE =
    "UPDATE app_settings SET language = ? WHERE id = 1";

// ============================================================================
// REGISTERS
// ============================================================================
const QString REGISTER_INSERT =
    "INSERT INTO registers (name, description, is_active, printer_name, cash_drawer_port) "
    "VALUES (?, ?, ?, ?, ?)";

const QString REGISTER_UPDATE =
    "UPDATE registers SET name = ?, description = ?, is_active = ?, printer_name = ?, cash_drawer_port = ? "
    "WHERE id = ?";

const QString REGISTER_DELETE =
    "DELETE FROM registers WHERE id = ?";

const QString REGISTER_SELECT_ALL =
    "SELECT id, name, description, is_active, printer_name, cash_drawer_port, created_at "
    "FROM registers ORDER BY name";

} // namespace Queries

#endif // QUERYREPOSITORY_H
