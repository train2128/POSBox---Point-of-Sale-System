#include "InventoryWindow.h"
#include "ui_Invenorywindow.h"
#include "InventoryUIMap.h"
#include "InventoryConstants.h"
#include "ProductDialog.h"
#include "UserSession.h"
#include "DatabaseManager.h"
#include "ChangeNotifier.h"
#include "QueryRepository.h"
#include "AccessControl.h"
#include "StyleManager.h"
#include "LogoutManager.h"
#include "Prefixes.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QResizeEvent>
#include <QColor>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QDateEdit>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QDebug>

InventoryWindow::InventoryWindow(UserSession* session, DatabaseManager* dbManager,
                                 ChangeNotifier* notifier, QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_session(session)
    , m_db(dbManager)
    , m_notifier(notifier)
    , m_searchTimer(new QTimer(this))
    , m_currentSearch("")
    , m_flashTimer(nullptr)
    , m_flashState(true)
    , m_currentTable(InventoryConstants::TABLE_PRODUCTS)
    , m_isEditingStock(false)
    , m_editingStockRow(-1)
    , m_blockCellChanged(false)
{
    ui->setupUi(this);
    InventoryUIMap::initialize(ui);
    setupUI();
    setupConnections();
    setupTableHeaders(m_currentTable);
    loadCurrentTable();
    showMaximized();
}

InventoryWindow::~InventoryWindow()
{
    // m_flashTimer is parented to this window, so Qt cleans it up automatically.
    // Setting to nullptr here is just for clarity.
    m_flashTimer = nullptr;
    delete ui;
}

void InventoryWindow::setupUI()
{
    setWindowTitle("POSBox - Inventory");

    m_searchTimer->setSingleShot(true);
    m_searchTimer->setInterval(AppConstants::SEARCH_DELAY_MS);

    if (centralWidget()) {
        centralWidget()->setObjectName("centralwidget");
    }

    if (m_session && InventoryUIMap::deleteBtn) {
        bool canDelete = AccessControl::canPerformAction(
            m_session->username(), AccessControl::DeleteProduct, m_db->database());
        InventoryUIMap::deleteBtn->setVisible(canDelete);
    }

    if (InventoryUIMap::mainTable) {
        QTableWidget* t  = InventoryUIMap::mainTable;
        QHeaderView*  hh = t->horizontalHeader();

        t->setSelectionBehavior(QAbstractItemView::SelectRows);
        t->setSelectionMode(QAbstractItemView::SingleSelection);
        t->setEditTriggers(QAbstractItemView::NoEditTriggers);
        t->verticalHeader()->setVisible(false);
        t->setAlternatingRowColors(true);

        hh->setSectionsMovable(false);
        hh->setSectionsClickable(false);
        t->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    }
}

void InventoryWindow::setupConnections()
{
    connect(InventoryUIMap::tableSelect, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &InventoryWindow::onTableSelectChanged);
    connect(InventoryUIMap::searchBox, &QLineEdit::textChanged,
            this, &InventoryWindow::onSearchChanged);
    connect(m_searchTimer, &QTimer::timeout,
            this, &InventoryWindow::onSearchTimer);
    connect(InventoryUIMap::addBtn, &QPushButton::clicked,
            this, &InventoryWindow::onAddClicked);
    connect(InventoryUIMap::editBtn, &QPushButton::clicked,
            this, &InventoryWindow::onEditClicked);
    connect(InventoryUIMap::deleteBtn, &QPushButton::clicked,
            this, &InventoryWindow::onDeleteClicked);
    connect(InventoryUIMap::logoutBtn, &QPushButton::clicked,
            this, &InventoryWindow::onLogoutClicked);
    connect(InventoryUIMap::mainTable, &QTableWidget::itemSelectionChanged,
            this, &InventoryWindow::onSelectionChanged);
    connect(InventoryUIMap::mainTable, &QTableWidget::cellDoubleClicked,
            this, &InventoryWindow::onCellDoubleClicked);
    connect(InventoryUIMap::mainTable, &QTableWidget::cellChanged,
            this, &InventoryWindow::onCellChanged);

    if (m_notifier) {
        connect(m_notifier, &ChangeNotifier::productChanged,
                this, [this](int) { if (m_currentTable == InventoryConstants::TABLE_PRODUCTS) loadCurrentTable(); });
        connect(m_notifier, &ChangeNotifier::categoryChanged,
                this, [this](int) { if (m_currentTable == InventoryConstants::TABLE_CATEGORIES) loadCurrentTable(); });
        connect(m_notifier, &ChangeNotifier::discountChanged,
                this, [this](int) { if (m_currentTable == InventoryConstants::TABLE_DISCOUNTS) loadCurrentTable(); });
    }
}

void InventoryWindow::setupTableHeaders(int tableIndex)
{
    if (!InventoryUIMap::mainTable) return;

    QTableWidget* t = InventoryUIMap::mainTable;
    QHeaderView*  h = t->horizontalHeader();

    t->clearContents();
    t->setRowCount(0);

    h->setSectionsMovable(false);
    h->setSectionsClickable(false);

    switch (tableIndex) {

    case InventoryConstants::TABLE_PRODUCTS:
        t->setColumnCount(InventoryConstants::PRODUCT_COL_COUNT);
        t->setHorizontalHeaderLabels({"ID", "Name", "Stock", "Cost", "Price"});
        h->setSectionResizeMode(InventoryConstants::COL_PRODUCT_ID,    QHeaderView::Fixed);
        h->setSectionResizeMode(InventoryConstants::COL_PRODUCT_NAME,  QHeaderView::Stretch);
        h->setSectionResizeMode(InventoryConstants::COL_PRODUCT_STOCK, QHeaderView::Fixed);
        h->setSectionResizeMode(InventoryConstants::COL_PRODUCT_COST,  QHeaderView::Fixed);
        h->setSectionResizeMode(InventoryConstants::COL_PRODUCT_PRICE, QHeaderView::Fixed);
        t->setColumnWidth(InventoryConstants::COL_PRODUCT_ID,    50);
        t->setColumnWidth(InventoryConstants::COL_PRODUCT_STOCK, 65);
        t->setColumnWidth(InventoryConstants::COL_PRODUCT_COST,  115);
        t->setColumnWidth(InventoryConstants::COL_PRODUCT_PRICE, 115);
        break;

    case InventoryConstants::TABLE_CATEGORIES:
        t->setColumnCount(InventoryConstants::CATEGORY_COL_COUNT);
        t->setHorizontalHeaderLabels({"Name", "Description"});
        h->setSectionResizeMode(InventoryConstants::COL_CATEGORY_NAME,        QHeaderView::Fixed);
        h->setSectionResizeMode(InventoryConstants::COL_CATEGORY_DESCRIPTION, QHeaderView::Stretch);
        applyCategoryColumnRatio();
        break;

    case InventoryConstants::TABLE_DISCOUNTS:
        t->setColumnCount(InventoryConstants::DISCOUNT_COL_COUNT);
        t->setHorizontalHeaderLabels({"Name", "Value", "Active", "End Date"});
        h->setSectionResizeMode(InventoryConstants::COL_DISCOUNT_NAME,     QHeaderView::Stretch);
        h->setSectionResizeMode(InventoryConstants::COL_DISCOUNT_VALUE,    QHeaderView::Stretch);
        h->setSectionResizeMode(InventoryConstants::COL_DISCOUNT_ACTIVE,   QHeaderView::Stretch);
        h->setSectionResizeMode(InventoryConstants::COL_DISCOUNT_END_DATE, QHeaderView::Stretch);
        break;
    }
}

void InventoryWindow::applyCategoryColumnRatio()
{
    if (!InventoryUIMap::mainTable) return;
    if (m_currentTable != InventoryConstants::TABLE_CATEGORIES) return;

    int totalWidth = InventoryUIMap::mainTable->viewport()->width();
    if (totalWidth <= 0) return;

    int nameWidth = totalWidth / 3;
    InventoryUIMap::mainTable->setColumnWidth(InventoryConstants::COL_CATEGORY_NAME, nameWidth);
}

void InventoryWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    applyCategoryColumnRatio();
}

// ===== TABLE SWITCHING & SEARCH =====

void InventoryWindow::onTableSelectChanged(int index)
{
    m_currentTable = index;
    InventoryUIMap::searchBox->clear();
    m_currentSearch.clear();
    setupTableHeaders(index);
    loadCurrentTable();

    if (InventoryUIMap::editBtn)   InventoryUIMap::editBtn->setEnabled(false);
    if (InventoryUIMap::deleteBtn) InventoryUIMap::deleteBtn->setEnabled(false);

    // Stop flashing when leaving the products table
    if (index != InventoryConstants::TABLE_PRODUCTS) {
        if (m_flashTimer) {
            m_flashTimer->stop();
            m_flashTimer->deleteLater();
            m_flashTimer = nullptr;
        }
    }
}

void InventoryWindow::onSearchChanged(const QString& text)
{
    m_currentSearch = text;
    m_searchTimer->start();
}

void InventoryWindow::onSearchTimer()
{
    loadCurrentTable();
}

void InventoryWindow::loadCurrentTable()
{
    switch (m_currentTable) {
    case InventoryConstants::TABLE_PRODUCTS:   loadProducts(m_currentSearch);   break;
    case InventoryConstants::TABLE_CATEGORIES: loadCategories(m_currentSearch); break;
    case InventoryConstants::TABLE_DISCOUNTS:  loadDiscounts(m_currentSearch);  break;
    }
}

// ===== DATA LOADING =====

void InventoryWindow::loadProducts(const QString& search)
{
    if (!m_db || !InventoryUIMap::mainTable) return;

    m_blockCellChanged = true;
    InventoryUIMap::mainTable->setRowCount(0);

    QSqlQuery query(m_db->database());

    QString sql = Queries::PRODUCT_SELECT_ALL;
    if (!search.isEmpty()) {
        sql = "SELECT p.id, p.name, p.description, p.cost, p.price, p.stock_quantity, p.track_stock, "
              "p.min_stock_alert, p.barcode, p.sku, p.category_id, c.name as category_name, "
              "p.is_service, p.has_variants, p.image_path, p.created_at, p.updated_at "
              "FROM products p LEFT JOIN categories c ON p.category_id = c.id "
              "WHERE p.name LIKE ? OR c.name LIKE ? "
              "ORDER BY p.name";
        query.prepare(sql);
        query.addBindValue("%" + search + "%");
        query.addBindValue("%" + search + "%");
    } else {
        query.prepare(sql);
    }

    if (!query.exec()) {
        qWarning() << "Failed to load products:" << query.lastError().text();
        m_blockCellChanged = false;
        return;
    }

    // ── Collect all rows from DB into buckets for sorting ────────────────────
    //
    // Sort order:
    //   Bucket 0 — out of stock  (tracked, non-service, non-variant, stock == 0)
    //   Bucket 1 — low stock     (tracked, non-service, non-variant, 1 <= stock <= LOW_STOCK_THRESHOLD)
    //   Bucket 2 — normal / no-stock-tracking items (services, variants, stock > LOW_STOCK_THRESHOLD)
    //
    // Within each bucket rows are kept in the DB ORDER BY p.name order (already alphabetical).
    // Services and variant products land in bucket 2 so they're interspersed alphabetically
    // among normal-stock items.

    struct ProductRow {
        int     id;
        QString name;
        int     stock;
        double  cost;
        double  price;
        bool    isService;
        bool    hasVariants;
        int     bucket; // 0 = out-of-stock, 1 = low, 2 = normal/no-stock
    };

    QList<ProductRow> bucket0; // out of stock
    QList<ProductRow> bucket1; // low stock
    QList<ProductRow> bucket2; // normal / service / variant

    while (query.next()) {
        ProductRow r;
        r.id          = query.value("id").toInt();
        r.name        = query.value("name").toString();
        r.stock       = query.value("stock_quantity").toInt();
        r.cost        = query.value("cost").toDouble();
        r.price       = query.value("price").toDouble();
        r.isService   = query.value("is_service").toBool();
        r.hasVariants = query.value("has_variants").toBool();

        // Only tracked, non-service, non-variant products participate in stock sorting
        bool stockTracked = !r.isService && !r.hasVariants;

        if (stockTracked && r.stock == InventoryConstants::OUT_OF_STOCK_VALUE) {
            r.bucket = 0;
            bucket0.append(r);
        } else if (stockTracked && r.stock <= InventoryConstants::LOW_STOCK_THRESHOLD) {
            r.bucket = 1;
            bucket1.append(r);
        } else {
            r.bucket = 2;
            bucket2.append(r);
        }
    }

    // ── Insert rows in sorted order: bucket0, bucket1, bucket2 ───────────────
    // Each bucket is already alphabetically sorted because the SQL uses ORDER BY p.name.

    auto insertRows = [&](const QList<ProductRow>& rows) {
        for (const ProductRow& r : rows) {
            int tableRow = InventoryUIMap::mainTable->rowCount();
            InventoryUIMap::mainTable->insertRow(tableRow);

            QTableWidgetItem* idItem = new QTableWidgetItem(QString::number(r.id));
            idItem->setTextAlignment(Qt::AlignCenter);
            InventoryUIMap::mainTable->setItem(tableRow, InventoryConstants::COL_PRODUCT_ID, idItem);

            QTableWidgetItem* nameItem = new QTableWidgetItem(r.name);
            nameItem->setTextAlignment(Qt::AlignCenter);
            InventoryUIMap::mainTable->setItem(tableRow, InventoryConstants::COL_PRODUCT_NAME, nameItem);

            // Stock cell: empty string for services and variant products
            QString stockText = (r.isService || r.hasVariants) ? "" : QString::number(r.stock);
            QTableWidgetItem* stockItem = new QTableWidgetItem(stockText);
            stockItem->setTextAlignment(Qt::AlignCenter);
            InventoryUIMap::mainTable->setItem(tableRow, InventoryConstants::COL_PRODUCT_STOCK, stockItem);

            // Cost / price: empty for variant products
            QTableWidgetItem* costItem = new QTableWidgetItem(
                r.hasVariants ? "" : AppHelpers::formatPrice(r.cost));
            costItem->setTextAlignment(Qt::AlignCenter);
            InventoryUIMap::mainTable->setItem(tableRow, InventoryConstants::COL_PRODUCT_COST, costItem);

            QTableWidgetItem* priceItem = new QTableWidgetItem(
                r.hasVariants ? "" : AppHelpers::formatPrice(r.price));
            priceItem->setTextAlignment(Qt::AlignCenter);
            InventoryUIMap::mainTable->setItem(tableRow, InventoryConstants::COL_PRODUCT_PRICE, priceItem);
        }
    };

    insertRows(bucket0);
    insertRows(bucket1);
    insertRows(bucket2);

    m_blockCellChanged = false;

    qDebug() << "Loaded" << InventoryUIMap::mainTable->rowCount()
             << "products (out-of-stock:" << bucket0.size()
             << "low:" << bucket1.size()
             << "normal:" << bucket2.size() << ")";

    // Apply text colors and manage the flash timer
    applyStockColors();
    updateFlashTimer();
}

void InventoryWindow::loadCategories(const QString& search)
{
    if (!m_db || !InventoryUIMap::mainTable) return;

    m_blockCellChanged = true;
    InventoryUIMap::mainTable->setRowCount(0);

    QSqlQuery query(m_db->database());
    if (!search.isEmpty()) {
        query.prepare("SELECT id, name, description FROM categories WHERE name LIKE ? ORDER BY name");
        query.addBindValue("%" + search + "%");
    } else {
        query.prepare("SELECT id, name, description FROM categories ORDER BY display_order, name");
    }

    if (!query.exec()) {
        qWarning() << "Failed to load categories:" << query.lastError().text();
        m_blockCellChanged = false;
        return;
    }

    while (query.next()) {
        int row = InventoryUIMap::mainTable->rowCount();
        InventoryUIMap::mainTable->insertRow(row);

        QTableWidgetItem* nameItem = new QTableWidgetItem(query.value("name").toString());
        nameItem->setData(Qt::UserRole, query.value("id").toInt());
        nameItem->setTextAlignment(Qt::AlignCenter);
        InventoryUIMap::mainTable->setItem(row, InventoryConstants::COL_CATEGORY_NAME, nameItem);

        QTableWidgetItem* descItem = new QTableWidgetItem(query.value("description").toString());
        descItem->setTextAlignment(Qt::AlignCenter);
        InventoryUIMap::mainTable->setItem(row, InventoryConstants::COL_CATEGORY_DESCRIPTION, descItem);
    }

    m_blockCellChanged = false;
    qDebug() << "Loaded" << InventoryUIMap::mainTable->rowCount() << "categories";
}

void InventoryWindow::loadDiscounts(const QString& search)
{
    if (!m_db || !InventoryUIMap::mainTable) return;

    m_blockCellChanged = true;
    InventoryUIMap::mainTable->setRowCount(0);

    QSqlQuery query(m_db->database());
    if (!search.isEmpty()) {
        query.prepare("SELECT id, name, type, value, is_active, end_date FROM discounts WHERE name LIKE ? ORDER BY name");
        query.addBindValue("%" + search + "%");
    } else {
        query.prepare("SELECT id, name, type, value, is_active, end_date FROM discounts ORDER BY name");
    }

    if (!query.exec()) {
        qWarning() << "Failed to load discounts:" << query.lastError().text();
        m_blockCellChanged = false;
        return;
    }

    while (query.next()) {
        int row = InventoryUIMap::mainTable->rowCount();
        InventoryUIMap::mainTable->insertRow(row);

        QString type    = query.value("type").toString();
        double  value   = query.value("value").toDouble();
        bool    isActive = query.value("is_active").toBool();
        QString endDate = query.value("end_date").toString();

        QString valueStr = (type == "percentage")
                               ? QString("%1%").arg(value, 0, 'f', 1)
                               : AppHelpers::formatPrice(value);

        QTableWidgetItem* nameItem = new QTableWidgetItem(query.value("name").toString());
        nameItem->setData(Qt::UserRole, query.value("id").toInt());
        nameItem->setTextAlignment(Qt::AlignCenter);
        InventoryUIMap::mainTable->setItem(row, InventoryConstants::COL_DISCOUNT_NAME, nameItem);

        QTableWidgetItem* valueItem = new QTableWidgetItem(valueStr);
        valueItem->setTextAlignment(Qt::AlignCenter);
        InventoryUIMap::mainTable->setItem(row, InventoryConstants::COL_DISCOUNT_VALUE, valueItem);

        QTableWidgetItem* activeItem = new QTableWidgetItem(isActive ? "Yes" : "No");
        activeItem->setTextAlignment(Qt::AlignCenter);
        InventoryUIMap::mainTable->setItem(row, InventoryConstants::COL_DISCOUNT_ACTIVE, activeItem);

        QTableWidgetItem* endDateItem = new QTableWidgetItem(endDate);
        endDateItem->setTextAlignment(Qt::AlignCenter);
        InventoryUIMap::mainTable->setItem(row, InventoryConstants::COL_DISCOUNT_END_DATE, endDateItem);
    }

    m_blockCellChanged = false;
    qDebug() << "Loaded" << InventoryUIMap::mainTable->rowCount() << "discounts";
}

// ===== STOCK COLOR HELPERS =====

QString InventoryWindow::defaultTextColor() const
{
    // Match the theme currently applied so the flash "off" color is always correct
    return (StyleManager::loadColorScheme() == StyleManager::ColorScheme::Light)
               ? QString(InventoryConstants::COLOR_TEXT_LIGHT_THEME)
               : QString(InventoryConstants::COLOR_TEXT_DARK_THEME);
}

void InventoryWindow::applyStockColors()
{
    QTableWidget* t = InventoryUIMap::mainTable;
    if (!t) return;

    const QColor yellowColor(InventoryConstants::COLOR_LOW_STOCK);
    const QColor redColor(InventoryConstants::COLOR_OUT_OF_STOCK);
    const QColor defaultColor(defaultTextColor());

    const int rowCount = t->rowCount();
    for (int row = 0; row < rowCount; ++row) {
        QTableWidgetItem* stockItem = t->item(row, InventoryConstants::COL_PRODUCT_STOCK);
        if (!stockItem) continue;

        const QString stockText = stockItem->text().trimmed();

        // Empty stock cell means service or variant product — skip coloring entirely
        if (stockText.isEmpty()) continue;

        bool ok = false;
        const int stock = stockText.toInt(&ok);
        if (!ok) continue;

        if (stock == InventoryConstants::OUT_OF_STOCK_VALUE) {
            // Out of stock — apply red to name and stock cells.
            // The flash timer will toggle between red and default; start in "on" (red) state.
            const QColor color = m_flashState ? redColor : defaultColor;
            if (t->item(row, InventoryConstants::COL_PRODUCT_NAME))
                t->item(row, InventoryConstants::COL_PRODUCT_NAME)->setForeground(color);
            stockItem->setForeground(color);

        } else if (stock <= InventoryConstants::LOW_STOCK_THRESHOLD) {
            // Low stock — steady yellow on name and stock cells
            if (t->item(row, InventoryConstants::COL_PRODUCT_NAME))
                t->item(row, InventoryConstants::COL_PRODUCT_NAME)->setForeground(yellowColor);
            stockItem->setForeground(yellowColor);

        } else {
            // Normal stock — restore default text color
            const QColor normal(defaultTextColor());
            if (t->item(row, InventoryConstants::COL_PRODUCT_NAME))
                t->item(row, InventoryConstants::COL_PRODUCT_NAME)->setForeground(normal);
            stockItem->setForeground(normal);
        }
    }
}

void InventoryWindow::updateFlashTimer()
{
    if (m_currentTable != InventoryConstants::TABLE_PRODUCTS) return;

    QTableWidget* t = InventoryUIMap::mainTable;
    if (!t) return;

    // Check whether any out-of-stock rows exist
    bool hasOutOfStock = false;
    const int rowCount = t->rowCount();
    for (int row = 0; row < rowCount; ++row) {
        QTableWidgetItem* stockItem = t->item(row, InventoryConstants::COL_PRODUCT_STOCK);
        if (!stockItem) continue;
        const QString stockText = stockItem->text().trimmed();
        if (stockText.isEmpty()) continue; // service / variant — skip
        bool ok = false;
        if (stockText.toInt(&ok) == InventoryConstants::OUT_OF_STOCK_VALUE && ok) {
            hasOutOfStock = true;
            break;
        }
    }

    if (hasOutOfStock) {
        // Start timer only if not already running
        if (!m_flashTimer) {
            m_flashTimer = new QTimer(this);
            m_flashTimer->setInterval(InventoryConstants::STOCK_FLASH_INTERVAL_MS);
            connect(m_flashTimer, &QTimer::timeout, this, &InventoryWindow::onFlashTick);
            m_flashState = true; // start in "red visible" state
            m_flashTimer->start();
            qDebug() << "Flash timer started (out-of-stock items present)";
        }
    } else {
        // No out-of-stock rows — stop and destroy the timer to free resources
        if (m_flashTimer) {
            m_flashTimer->stop();
            m_flashTimer->deleteLater();
            m_flashTimer = nullptr;
            qDebug() << "Flash timer stopped (no out-of-stock items)";
        }
    }
}

void InventoryWindow::onFlashTick()
{
    // Toggle flash state and repaint only the out-of-stock rows
    m_flashState = !m_flashState;

    QTableWidget* t = InventoryUIMap::mainTable;
    if (!t) return;

    // Only update if we're still on the products table
    if (m_currentTable != InventoryConstants::TABLE_PRODUCTS) return;

    const QColor redColor(InventoryConstants::COLOR_OUT_OF_STOCK);
    const QColor defaultColor(defaultTextColor());
    const QColor activeColor = m_flashState ? redColor : defaultColor;

    const int rowCount = t->rowCount();
    for (int row = 0; row < rowCount; ++row) {
        QTableWidgetItem* stockItem = t->item(row, InventoryConstants::COL_PRODUCT_STOCK);
        if (!stockItem) continue;
        const QString stockText = stockItem->text().trimmed();
        if (stockText.isEmpty()) continue; // service / variant — skip

        bool ok = false;
        if (stockText.toInt(&ok) == InventoryConstants::OUT_OF_STOCK_VALUE && ok) {
            if (t->item(row, InventoryConstants::COL_PRODUCT_NAME))
                t->item(row, InventoryConstants::COL_PRODUCT_NAME)->setForeground(activeColor);
            stockItem->setForeground(activeColor);
        }
    }
}

// ===== SELECTION =====

void InventoryWindow::onSelectionChanged()
{
    int row = getSelectedRow();
    bool hasSelection = (row >= 0);

    if (InventoryUIMap::editBtn)   InventoryUIMap::editBtn->setEnabled(hasSelection);
    if (InventoryUIMap::deleteBtn) InventoryUIMap::deleteBtn->setEnabled(hasSelection);

    if (m_isEditingStock && hasSelection && row != m_editingStockRow) {
        m_isEditingStock  = false;
        m_editingStockRow = -1;
        InventoryUIMap::mainTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        loadProducts(m_currentSearch);
    }
}

// ===== INLINE STOCK EDIT =====

void InventoryWindow::onCellDoubleClicked(int row, int column)
{
    if (m_currentTable != InventoryConstants::TABLE_PRODUCTS) return;
    if (column != InventoryConstants::COL_PRODUCT_STOCK) return;

    QTableWidgetItem* stockItem = InventoryUIMap::mainTable->item(row, InventoryConstants::COL_PRODUCT_STOCK);
    if (!stockItem || stockItem->text().isEmpty()) return;

    m_isEditingStock  = true;
    m_editingStockRow = row;

    InventoryUIMap::mainTable->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::AnyKeyPressed);
    InventoryUIMap::mainTable->editItem(stockItem);
}

void InventoryWindow::onCellChanged(int row, int column)
{
    if (m_blockCellChanged) return;
    if (!m_isEditingStock) return;
    if (row != m_editingStockRow) return;
    if (column != InventoryConstants::COL_PRODUCT_STOCK) return;

    QTableWidgetItem* stockItem = InventoryUIMap::mainTable->item(row, column);
    if (!stockItem) return;

    bool ok = false;
    int newStock = stockItem->text().toInt(&ok);

    if (!ok || newStock < 0) {
        AppHelpers::showWarning(this, InventoryConstants::MSG_STOCK_INVALID);
        loadProducts(m_currentSearch);
        m_isEditingStock  = false;
        m_editingStockRow = -1;
        InventoryUIMap::mainTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        return;
    }

    int productId = getProductIdFromRow(row);
    if (productId <= 0) return;

    QSqlQuery query(m_db->database());
    query.prepare("UPDATE products SET stock_quantity = ? WHERE id = ?");
    query.addBindValue(newStock);
    query.addBindValue(productId);

    if (!query.exec()) {
        qWarning() << "Failed to update stock:" << query.lastError().text();
        AppHelpers::showError(this, "Failed to update stock.");
        loadProducts(m_currentSearch);
    } else {
        qDebug() << "Stock updated for product" << productId << "to" << newStock;
        if (m_notifier) {
            m_notifier->notifyChange(ChangeType::PRODUCT_CHANGED, productId);
        }
    }

    m_isEditingStock  = false;
    m_editingStockRow = -1;
    InventoryUIMap::mainTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
}

// ===== ADD / EDIT / DELETE ROUTING =====

void InventoryWindow::onAddClicked()
{
    switch (m_currentTable) {
    case InventoryConstants::TABLE_PRODUCTS:   addProduct();   break;
    case InventoryConstants::TABLE_CATEGORIES: addCategory();  break;
    case InventoryConstants::TABLE_DISCOUNTS:  addDiscount();  break;
    }
}

void InventoryWindow::onEditClicked()
{
    int row = getSelectedRow();
    if (row < 0) return;
    switch (m_currentTable) {
    case InventoryConstants::TABLE_PRODUCTS:   editProduct(row);   break;
    case InventoryConstants::TABLE_CATEGORIES: editCategory(row);  break;
    case InventoryConstants::TABLE_DISCOUNTS:  editDiscount(row);  break;
    }
}

void InventoryWindow::onDeleteClicked()
{
    int row = getSelectedRow();
    if (row < 0) return;

    if (m_session && !AccessControl::canPerformAction(
            m_session->username(), AccessControl::DeleteProduct, m_db->database())) {
        AppHelpers::showWarning(this, "You do not have permission to delete products.");
        return;
    }

    switch (m_currentTable) {
    case InventoryConstants::TABLE_PRODUCTS:   deleteProduct(row);   break;
    case InventoryConstants::TABLE_CATEGORIES: deleteCategory(row);  break;
    case InventoryConstants::TABLE_DISCOUNTS:  deleteDiscount(row);  break;
    }
}

void InventoryWindow::onLogoutClicked()
{
    if (!LogoutManager::confirmAndLogout(this, m_session->sessionId(), m_session->username())) {
        return;
    }
    close();
    emit logoutRequested();
}

// ===== PRODUCTS CRUD =====

void InventoryWindow::addProduct()
{
    ProductDialog dialog(m_db, this);
    if (dialog.exec() == QDialog::Accepted) {
        loadProducts(m_currentSearch);
        if (m_notifier) m_notifier->notifyChange(ChangeType::PRODUCT_CHANGED, -1);
        AppHelpers::showInfo(this, InventoryConstants::MSG_PRODUCT_ADD_SUCCESS);
    }
}

void InventoryWindow::editProduct(int row)
{
    int productId = getProductIdFromRow(row);
    if (productId <= 0) return;

    ProductDialog dialog(m_db, productId, this);
    if (dialog.exec() == QDialog::Accepted) {
        loadProducts(m_currentSearch);
        if (m_notifier) m_notifier->notifyChange(ChangeType::PRODUCT_CHANGED, productId);
        AppHelpers::showInfo(this, InventoryConstants::MSG_PRODUCT_EDIT_SUCCESS);
    }
}

void InventoryWindow::deleteProduct(int row)
{
    int productId = getProductIdFromRow(row);
    if (productId <= 0) return;

    if (!AppHelpers::confirmAction(this, InventoryConstants::MSG_PRODUCT_DELETE_CONFIRM)) return;

    QSqlQuery query(m_db->database());
    query.prepare(Queries::PRODUCT_DELETE);
    query.addBindValue(productId);

    if (!query.exec()) {
        AppHelpers::showError(this, "Failed to delete product: " + query.lastError().text());
        return;
    }

    loadProducts(m_currentSearch);
    if (m_notifier) m_notifier->notifyChange(ChangeType::PRODUCT_CHANGED, productId);
    AppHelpers::showInfo(this, InventoryConstants::MSG_PRODUCT_DELETE_SUCCESS);
}

// ===== CATEGORIES CRUD =====

void InventoryWindow::addCategory()
{
    if (m_session && !AccessControl::canPerformAction(
            m_session->username(), AccessControl::AddCategory, m_db->database())) {
        AppHelpers::showWarning(this, "You do not have permission to add categories.");
        return;
    }

    // ── Single dialog: name + description ────────────────────────────────────
    QDialog dlg(this);
    dlg.setWindowTitle("Add Category");
    dlg.setFixedWidth(340);
    dlg.setModal(true);

    QVBoxLayout* main = new QVBoxLayout(&dlg);
    main->setContentsMargins(16, 16, 16, 16);
    main->setSpacing(12);

    QFormLayout* form = new QFormLayout();
    form->setSpacing(8);

    QLineEdit* nameEdit = new QLineEdit(&dlg);
    nameEdit->setPlaceholderText("Category name");
    nameEdit->setAlignment(Qt::AlignCenter);
    form->addRow("Name:", nameEdit);

    QLineEdit* descEdit = new QLineEdit(&dlg);
    descEdit->setPlaceholderText("Description (optional)");
    descEdit->setAlignment(Qt::AlignCenter);
    form->addRow("Description:", descEdit);

    main->addLayout(form);

    QDialogButtonBox* btns = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    btns->button(QDialogButtonBox::Ok)->setText("Add");
    btns->button(QDialogButtonBox::Cancel)->setText("Cancel");
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    main->addWidget(btns);

    nameEdit->setFocus();

    if (dlg.exec() != QDialog::Accepted) return;

    const QString name = nameEdit->text().trimmed();
    if (name.isEmpty()) {
        AppHelpers::showWarning(this, InventoryConstants::MSG_CATEGORY_NAME_EMPTY);
        return;
    }
    const QString description = descEdit->text().trimmed();

    QSqlQuery query(m_db->database());
    query.prepare(Queries::CATEGORY_INSERT);
    query.addBindValue(name);
    query.addBindValue(description.isEmpty() ? QVariant() : QVariant(description));
    query.addBindValue(0);
    query.addBindValue(QVariant());
    query.addBindValue(QVariant());
    query.addBindValue(true);
    query.addBindValue(true);

    if (!query.exec()) {
        AppHelpers::showError(this, "Failed to add category: " + query.lastError().text());
        return;
    }

    int categoryId = query.lastInsertId().toInt();
    loadCategories(m_currentSearch);
    if (m_notifier) m_notifier->notifyChange(ChangeType::CATEGORY_CHANGED, categoryId);
    AppHelpers::showInfo(this, InventoryConstants::MSG_CATEGORY_ADD_SUCCESS);
}

void InventoryWindow::editCategory(int row)
{
    int categoryId = getCategoryIdFromRow(row);
    if (categoryId <= 0) return;

    const QString currentName = InventoryUIMap::mainTable->item(row, InventoryConstants::COL_CATEGORY_NAME)->text();
    const QString currentDesc = InventoryUIMap::mainTable->item(row, InventoryConstants::COL_CATEGORY_DESCRIPTION)->text();

    // ── Single dialog: name + description, pre-filled ────────────────────────
    QDialog dlg(this);
    dlg.setWindowTitle("Edit Category");
    dlg.setFixedWidth(340);
    dlg.setModal(true);

    QVBoxLayout* main = new QVBoxLayout(&dlg);
    main->setContentsMargins(16, 16, 16, 16);
    main->setSpacing(12);

    QFormLayout* form = new QFormLayout();
    form->setSpacing(8);

    QLineEdit* nameEdit = new QLineEdit(currentName, &dlg);
    nameEdit->setAlignment(Qt::AlignCenter);
    form->addRow("Name:", nameEdit);

    QLineEdit* descEdit = new QLineEdit(currentDesc, &dlg);
    descEdit->setPlaceholderText("Description (optional)");
    descEdit->setAlignment(Qt::AlignCenter);
    form->addRow("Description:", descEdit);

    main->addLayout(form);

    QDialogButtonBox* btns = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    btns->button(QDialogButtonBox::Ok)->setText("Save");
    btns->button(QDialogButtonBox::Cancel)->setText("Cancel");
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    main->addWidget(btns);

    nameEdit->setFocus();
    nameEdit->selectAll();

    if (dlg.exec() != QDialog::Accepted) return;

    const QString name = nameEdit->text().trimmed();
    if (name.isEmpty()) {
        AppHelpers::showWarning(this, InventoryConstants::MSG_CATEGORY_NAME_EMPTY);
        return;
    }
    const QString description = descEdit->text().trimmed();

    QSqlQuery query(m_db->database());
    query.prepare(Queries::CATEGORY_UPDATE);
    query.addBindValue(name);
    query.addBindValue(description.isEmpty() ? QVariant() : QVariant(description));
    query.addBindValue(0);
    query.addBindValue(QVariant());
    query.addBindValue(QVariant());
    query.addBindValue(true);
    query.addBindValue(true);
    query.addBindValue(categoryId);

    if (!query.exec()) {
        AppHelpers::showError(this, "Failed to update category: " + query.lastError().text());
        return;
    }

    loadCategories(m_currentSearch);
    if (m_notifier) m_notifier->notifyChange(ChangeType::CATEGORY_CHANGED, categoryId);
    AppHelpers::showInfo(this, InventoryConstants::MSG_CATEGORY_EDIT_SUCCESS);
}

void InventoryWindow::deleteCategory(int row)
{
    int categoryId = getCategoryIdFromRow(row);
    if (categoryId <= 0) return;

    QSqlQuery countQuery(m_db->database());
    countQuery.prepare("SELECT COUNT(*) FROM products WHERE category_id = ?");
    countQuery.addBindValue(categoryId);
    countQuery.exec();
    countQuery.next();
    int count = countQuery.value(0).toInt();

    QString message = count > 0
                          ? QString("This category is assigned to %1 product(s). They will become uncategorized. Continue?").arg(count)
                          : "Are you sure you want to delete this category?";

    if (!AppHelpers::confirmAction(this, message)) return;

    QSqlQuery query(m_db->database());
    query.prepare(Queries::CATEGORY_DELETE);
    query.addBindValue(categoryId);

    if (!query.exec()) {
        AppHelpers::showError(this, "Failed to delete category: " + query.lastError().text());
        return;
    }

    loadCategories(m_currentSearch);
    if (m_notifier) m_notifier->notifyChange(ChangeType::CATEGORY_CHANGED, categoryId);
    AppHelpers::showInfo(this, InventoryConstants::MSG_CATEGORY_DELETE_SUCCESS);
}

// ===== DISCOUNTS CRUD =====

void InventoryWindow::addDiscount()
{
    if (m_session && !AccessControl::canPerformAction(
            m_session->username(), AccessControl::ManageDiscounts, m_db->database())) {
        AppHelpers::showWarning(this, "You do not have permission to manage discounts.");
        return;
    }

    // ── Single dialog: all discount fields ───────────────────────────────────
    QDialog dlg(this);
    dlg.setWindowTitle("Add Discount");
    dlg.setFixedWidth(360);
    dlg.setModal(true);

    QVBoxLayout* main = new QVBoxLayout(&dlg);
    main->setContentsMargins(16, 16, 16, 16);
    main->setSpacing(12);

    QFormLayout* form = new QFormLayout();
    form->setSpacing(8);

    // Name
    QLineEdit* nameEdit = new QLineEdit(&dlg);
    nameEdit->setPlaceholderText("Discount name");
    nameEdit->setAlignment(Qt::AlignCenter);
    form->addRow("Name:", nameEdit);

    // Type
    QComboBox* typeCombo = new QComboBox(&dlg);
    typeCombo->addItem("Percentage (%)", "percentage");
    typeCombo->addItem("Fixed Amount",   "amount");
    form->addRow("Type:", typeCombo);

    // Value
    QDoubleSpinBox* valueSpin = new QDoubleSpinBox(&dlg);
    valueSpin->setMinimum(0.0);
    valueSpin->setMaximum(9999999.0);
    valueSpin->setDecimals(3);
    valueSpin->setValue(0.0);
    form->addRow("Value:", valueSpin);

    // Active
    QCheckBox* activeCheck = new QCheckBox("Active", &dlg);
    activeCheck->setChecked(true);
    form->addRow("Status:", activeCheck);

    // Start date (optional)
    QCheckBox* startCheck = new QCheckBox("Set start date", &dlg);
    QDateEdit* startEdit  = new QDateEdit(QDate::currentDate(), &dlg);
    startEdit->setCalendarPopup(true);
    startEdit->setDisplayFormat("dd/MM/yyyy");
    startEdit->setEnabled(false);
    connect(startCheck, &QCheckBox::toggled, startEdit, &QDateEdit::setEnabled);
    form->addRow(startCheck, startEdit);

    // End date (optional)
    QCheckBox* endCheck = new QCheckBox("Set end date", &dlg);
    QDateEdit* endEdit  = new QDateEdit(QDate::currentDate(), &dlg);
    endEdit->setCalendarPopup(true);
    endEdit->setDisplayFormat("dd/MM/yyyy");
    endEdit->setEnabled(false);
    connect(endCheck, &QCheckBox::toggled, endEdit, &QDateEdit::setEnabled);
    form->addRow(endCheck, endEdit);

    main->addLayout(form);

    QDialogButtonBox* btns = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    btns->button(QDialogButtonBox::Ok)->setText("Add");
    btns->button(QDialogButtonBox::Cancel)->setText("Cancel");
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    main->addWidget(btns);

    nameEdit->setFocus();

    if (dlg.exec() != QDialog::Accepted) return;

    const QString name = nameEdit->text().trimmed();
    if (name.isEmpty()) {
        AppHelpers::showWarning(this, InventoryConstants::MSG_DISCOUNT_NAME_EMPTY);
        return;
    }

    const QString type    = typeCombo->currentData().toString();
    const double  value   = valueSpin->value();
    const bool    isActive = activeCheck->isChecked();
    const QVariant startDate = startCheck->isChecked()
                                   ? QVariant(startEdit->date().toString("yyyy-MM-dd"))
                                   : QVariant();
    const QVariant endDate   = endCheck->isChecked()
                                 ? QVariant(endEdit->date().toString("yyyy-MM-dd"))
                                 : QVariant();

    QSqlQuery query(m_db->database());
    query.prepare(Queries::DISCOUNT_INSERT);
    query.addBindValue(name);
    query.addBindValue(type);
    query.addBindValue(value);
    query.addBindValue(isActive ? 1 : 0);
    query.addBindValue(startDate);
    query.addBindValue(endDate);

    if (!query.exec()) {
        AppHelpers::showError(this, "Failed to add discount: " + query.lastError().text());
        return;
    }

    int discountId = query.lastInsertId().toInt();
    loadDiscounts(m_currentSearch);
    if (m_notifier) m_notifier->notifyChange(ChangeType::DISCOUNT_CHANGED, discountId);
    AppHelpers::showInfo(this, InventoryConstants::MSG_DISCOUNT_ADD_SUCCESS);
}

void InventoryWindow::editDiscount(int row)
{
    if (m_session && !AccessControl::canPerformAction(
            m_session->username(), AccessControl::ManageDiscounts, m_db->database())) {
        AppHelpers::showWarning(this, "You do not have permission to manage discounts.");
        return;
    }

    int discountId = getDiscountIdFromRow(row);
    if (discountId <= 0) return;

    // Load current values from DB
    QSqlQuery loadQuery(m_db->database());
    loadQuery.prepare(
        "SELECT name, type, value, is_active, start_date, end_date "
        "FROM discounts WHERE id = ?");
    loadQuery.addBindValue(discountId);
    if (!loadQuery.exec() || !loadQuery.next()) return;

    const QString savedName      = loadQuery.value("name").toString();
    const QString savedType      = loadQuery.value("type").toString();
    const double  savedValue     = loadQuery.value("value").toDouble();
    const bool    savedActive    = loadQuery.value("is_active").toBool();
    const QString savedStartDate = loadQuery.value("start_date").toString();
    const QString savedEndDate   = loadQuery.value("end_date").toString();

    // ── Single dialog: all fields pre-filled ─────────────────────────────────
    QDialog dlg(this);
    dlg.setWindowTitle("Edit Discount");
    dlg.setFixedWidth(360);
    dlg.setModal(true);

    QVBoxLayout* main = new QVBoxLayout(&dlg);
    main->setContentsMargins(16, 16, 16, 16);
    main->setSpacing(12);

    QFormLayout* form = new QFormLayout();
    form->setSpacing(8);

    // Name
    QLineEdit* nameEdit = new QLineEdit(savedName, &dlg);
    nameEdit->setAlignment(Qt::AlignCenter);
    form->addRow("Name:", nameEdit);

    // Type
    QComboBox* typeCombo = new QComboBox(&dlg);
    typeCombo->addItem("Percentage (%)", "percentage");
    typeCombo->addItem("Fixed Amount",   "amount");
    typeCombo->setCurrentIndex(savedType == "percentage" ? 0 : 1);
    form->addRow("Type:", typeCombo);

    // Value
    QDoubleSpinBox* valueSpin = new QDoubleSpinBox(&dlg);
    valueSpin->setMinimum(0.0);
    valueSpin->setMaximum(9999999.0);
    valueSpin->setDecimals(3);
    valueSpin->setValue(savedValue);
    form->addRow("Value:", valueSpin);

    // Active
    QCheckBox* activeCheck = new QCheckBox("Active", &dlg);
    activeCheck->setChecked(savedActive);
    form->addRow("Status:", activeCheck);

    // Start date (optional — pre-fill if saved)
    QCheckBox* startCheck = new QCheckBox("Set start date", &dlg);
    QDateEdit* startEdit  = new QDateEdit(&dlg);
    startEdit->setCalendarPopup(true);
    startEdit->setDisplayFormat("dd/MM/yyyy");
    if (!savedStartDate.isEmpty()) {
        startCheck->setChecked(true);
        startEdit->setDate(QDate::fromString(savedStartDate, "yyyy-MM-dd"));
        startEdit->setEnabled(true);
    } else {
        startCheck->setChecked(false);
        startEdit->setDate(QDate::currentDate());
        startEdit->setEnabled(false);
    }
    connect(startCheck, &QCheckBox::toggled, startEdit, &QDateEdit::setEnabled);
    form->addRow(startCheck, startEdit);

    // End date (optional — pre-fill if saved)
    QCheckBox* endCheck = new QCheckBox("Set end date", &dlg);
    QDateEdit* endEdit  = new QDateEdit(&dlg);
    endEdit->setCalendarPopup(true);
    endEdit->setDisplayFormat("dd/MM/yyyy");
    if (!savedEndDate.isEmpty()) {
        endCheck->setChecked(true);
        endEdit->setDate(QDate::fromString(savedEndDate, "yyyy-MM-dd"));
        endEdit->setEnabled(true);
    } else {
        endCheck->setChecked(false);
        endEdit->setDate(QDate::currentDate());
        endEdit->setEnabled(false);
    }
    connect(endCheck, &QCheckBox::toggled, endEdit, &QDateEdit::setEnabled);
    form->addRow(endCheck, endEdit);

    main->addLayout(form);

    QDialogButtonBox* btns = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    btns->button(QDialogButtonBox::Ok)->setText("Save");
    btns->button(QDialogButtonBox::Cancel)->setText("Cancel");
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    main->addWidget(btns);

    nameEdit->setFocus();
    nameEdit->selectAll();

    if (dlg.exec() != QDialog::Accepted) return;

    const QString name = nameEdit->text().trimmed();
    if (name.isEmpty()) {
        AppHelpers::showWarning(this, InventoryConstants::MSG_DISCOUNT_NAME_EMPTY);
        return;
    }

    const QString type     = typeCombo->currentData().toString();
    const double  value    = valueSpin->value();
    const bool    isActive = activeCheck->isChecked();
    const QVariant startDate = startCheck->isChecked()
                                   ? QVariant(startEdit->date().toString("yyyy-MM-dd"))
                                   : QVariant();
    const QVariant endDate   = endCheck->isChecked()
                                 ? QVariant(endEdit->date().toString("yyyy-MM-dd"))
                                 : QVariant();

    QSqlQuery query(m_db->database());
    query.prepare(Queries::DISCOUNT_UPDATE);
    query.addBindValue(name);
    query.addBindValue(type);
    query.addBindValue(value);
    query.addBindValue(isActive ? 1 : 0);
    query.addBindValue(startDate);
    query.addBindValue(endDate);
    query.addBindValue(discountId);

    if (!query.exec()) {
        AppHelpers::showError(this, "Failed to update discount: " + query.lastError().text());
        return;
    }

    loadDiscounts(m_currentSearch);
    if (m_notifier) m_notifier->notifyChange(ChangeType::DISCOUNT_CHANGED, discountId);
    AppHelpers::showInfo(this, InventoryConstants::MSG_DISCOUNT_EDIT_SUCCESS);
}

void InventoryWindow::deleteDiscount(int row)
{
    if (m_session && !AccessControl::canPerformAction(
            m_session->username(), AccessControl::ManageDiscounts, m_db->database())) {
        AppHelpers::showWarning(this, "You do not have permission to manage discounts.");
        return;
    }

    int discountId = getDiscountIdFromRow(row);
    if (discountId <= 0) return;

    if (!AppHelpers::confirmAction(this, InventoryConstants::MSG_DISCOUNT_DELETE_CONFIRM)) return;

    QSqlQuery query(m_db->database());
    query.prepare(Queries::DISCOUNT_DELETE);
    query.addBindValue(discountId);

    if (!query.exec()) {
        AppHelpers::showError(this, "Failed to delete discount: " + query.lastError().text());
        return;
    }

    loadDiscounts(m_currentSearch);
    if (m_notifier) m_notifier->notifyChange(ChangeType::DISCOUNT_CHANGED, discountId);
    AppHelpers::showInfo(this, InventoryConstants::MSG_DISCOUNT_DELETE_SUCCESS);
}

// ===== HELPERS =====

int InventoryWindow::getSelectedRow() const
{
    if (!InventoryUIMap::mainTable) return -1;
    QList<QTableWidgetItem*> selected = InventoryUIMap::mainTable->selectedItems();
    if (selected.isEmpty()) return -1;
    return selected.first()->row();
}

int InventoryWindow::getProductIdFromRow(int row) const
{
    QTableWidgetItem* item = InventoryUIMap::mainTable->item(row, InventoryConstants::COL_PRODUCT_ID);
    if (!item) return -1;
    return item->text().toInt();
}

int InventoryWindow::getCategoryIdFromRow(int row) const
{
    QTableWidgetItem* item = InventoryUIMap::mainTable->item(row, InventoryConstants::COL_CATEGORY_NAME);
    if (!item) return -1;
    return item->data(Qt::UserRole).toInt();
}

int InventoryWindow::getDiscountIdFromRow(int row) const
{
    QTableWidgetItem* item = InventoryUIMap::mainTable->item(row, InventoryConstants::COL_DISCOUNT_NAME);
    if (!item) return -1;
    return item->data(Qt::UserRole).toInt();
}
