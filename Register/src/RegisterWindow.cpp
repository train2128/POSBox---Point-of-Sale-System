#include "RegisterWindow.h"
#include "ui_RegisterWindow.h"
#include "UIMap.h"
#include "RegisterConstants.h"
#include "Prefixes.h"
#include "CartItemWidget.h"
#include "DiscountItemWidget.h"
#include "ProductButtonCreator.h"
#include "VariantPickerDialog.h"
#include "DiscountDialog.h"
#include "SaveTicketDialog.h"
#include "OpenTicketsDialog.h"
#include "UserSession.h"
#include "DatabaseManager.h"
#include "ChangeNotifier.h"
#include "QueryRepository.h"
#include "SessionManager.h"
#include "StyleManager.h"
#include "TicketEditRequest.h"
#include "TicketsLock.h"
#include "CommonConfig.h"
#include "NotificationManager.h"
#include "NotificationPanel.h"
#include "AccessControl.h"
#include "LogoutManager.h"
#include "ThermalPrinter.h"
#include "TranslationManager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QVBoxLayout>
#include <QTimer>
#include <QScrollArea>
#include <QFrame>
#include <QSet>
#include <QApplication>
#include <QCoreApplication>
#include <QProcess>
#include <QScreen>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <QInputDialog>
#include <QMessageBox>

RegisterWindow::RegisterWindow(UserSession* session, DatabaseManager* dbManager,
                               ChangeNotifier* notifier, QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::RegisterWindow)
    , m_session(session)
    , m_db(dbManager)
    , m_notifier(notifier)
    , m_searchTimer(new QTimer(this))
    , m_currentSearchText("")
    , m_currentCategoryFilter(-1)
    , m_cartTotal(0.0)
    , m_customItemCounter(-1)
    , m_editMode(false)
    , m_editTicketId(-1)
    , m_openTicketEditMode(false)
    , m_openTicketEditId(-1)
    , m_openTicketAdvance(0.0)
    , m_notificationManager(nullptr)
    , m_notificationPanel(nullptr)
{
    ui->setupUi(this);

    // Apply global styles
    StyleManager::applyGlobalStyles(this, m_db->database());

    // Initialize UI mapping
    UIMap::initialize(ui);

    // Setup notification system
    m_notificationManager = new NotificationManager(dbManager, this);
    m_notificationManager->checkAndGenerate();
    connect(m_notificationManager, &NotificationManager::notificationsChanged, this, &RegisterWindow::updateNotificationBadge);
    updateNotificationBadge();

    // Setup
    setupUI();
    setupConnections();

    // Load data
    loadCategories();
    loadProducts();

    // Show maximized
    showMaximized();
}

RegisterWindow::~RegisterWindow()
{
    delete ui;
}

void RegisterWindow::setupUI()
{
    // Configure search timer
    m_searchTimer->setSingleShot(true);
    m_searchTimer->setInterval(AppConstants::SEARCH_DELAY_MS);

    // Update user display
    updateUserDisplay();

    // Apply translatable strings
    retranslateUi();
}

void RegisterWindow::retranslateUi()
{
    setWindowTitle(tr("POSBox - Register"));

    // Top bar
    if (UIMap::ticketsBtn)
        UIMap::ticketsBtn->setText(tr("Tickets"));
    if (UIMap::notificationBtn)
        UIMap::notificationBtn->setToolTip(tr("Notifications"));
    if (UIMap::logoutBtn)
        UIMap::logoutBtn->setToolTip(tr("Logout"));

    // Products section
    if (UIMap::searchBar)
        UIMap::searchBar->setPlaceholderText(tr("Search products or scan barcode..."));
    if (UIMap::customItemBtn)
        UIMap::customItemBtn->setText(tr("Custom Item"));

    // Cart section
    if (UIMap::withdrawBtn)
        UIMap::withdrawBtn->setText(tr("Withdraw"));
    if (UIMap::discountBtn)
        UIMap::discountBtn->setToolTip(tr("Apply Discount"));
    if (UIMap::confirmBtn)
        UIMap::confirmBtn->setText(tr("Confirm Sale"));
    if (UIMap::cancelBtn)
        UIMap::cancelBtn->setText(tr("Cancel"));

    // Update "All Categories" item in filterBox
    if (UIMap::filterBox && UIMap::filterBox->count() > 0) {
        UIMap::filterBox->setItemText(0, tr("All Categories"));
    }

    updateCartTotal();
    updateOpenTicketButton();
}

void RegisterWindow::setupConnections()
{
    // Top bar
    connect(UIMap::ticketsBtn, &QPushButton::clicked, this, &RegisterWindow::onTicketsClicked);
    connect(UIMap::notificationBtn, &QPushButton::clicked, this, &RegisterWindow::onNotificationClicked);
    connect(UIMap::logoutBtn, &QPushButton::clicked, this, &RegisterWindow::onLogoutClicked);

    // Products section
    connect(UIMap::searchBar, &QLineEdit::textChanged, this, &RegisterWindow::onSearchChanged);
    connect(UIMap::filterBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RegisterWindow::onCategoryChanged);
    connect(UIMap::customItemBtn, &QPushButton::clicked, this, &RegisterWindow::onCustomItemClicked);
    connect(m_searchTimer, &QTimer::timeout, this, &RegisterWindow::onSearchTimer);

    // Cart section
    connect(UIMap::discountBtn, &QPushButton::clicked, this, &RegisterWindow::onDiscountClicked);
    connect(UIMap::openTicketsBtn, &QPushButton::clicked, this, &RegisterWindow::onOpenTicketsClicked);
    connect(UIMap::withdrawBtn, &QPushButton::clicked, this, &RegisterWindow::onWithdrawClicked);
    connect(UIMap::confirmBtn, &QPushButton::clicked, this, &RegisterWindow::onConfirmClicked);
    connect(UIMap::cancelBtn, &QPushButton::clicked, this, &RegisterWindow::onCancelClicked);

    // Language change notifications
    connect(&TranslationManager::instance(), &TranslationManager::languageChanged,
            this, &RegisterWindow::retranslateUi);

    // Change notifications
    if (m_notifier) {
        connect(m_notifier, &ChangeNotifier::productChanged,
                this, [this](int) { loadProducts(); });
        connect(m_notifier, &ChangeNotifier::categoryChanged,
                this, [this](int) { loadCategories(); });
        connect(m_notifier, &ChangeNotifier::ticketEditRequested,
                this, [this](int) { checkPendingEditRequest(); });
    }
}

void RegisterWindow::updateUserDisplay()
{
    if (m_session && UIMap::userLabel) {
        QString displayText = QString(tr("%1 (%2)"))
        .arg(m_session->displayName())
            .arg(m_session->role());
        UIMap::userLabel->setText(displayText);
    }
}

void RegisterWindow::loadCategories()
{
    if (!m_db || !UIMap::filterBox) return;

    m_categories.clear();
    UIMap::filterBox->clear();

    QSqlQuery query(m_db->database());
    query.prepare(Queries::CATEGORY_SELECT_ALL);

    if (!query.exec()) {
        qWarning() << "Failed to load categories:" << query.lastError().text();
        return;
    }

    UIMap::filterBox->addItem(tr("All Categories"), -1);

    while (query.next()) {
        QHash<QString, QVariant> category;
        category["id"] = query.value("id");
        category["name"] = query.value("name");
        category["description"] = query.value("description");

        m_categories.append(category);
        UIMap::filterBox->addItem(query.value("name").toString(), query.value("id"));
    }

    qDebug() << "Loaded" << m_categories.size() << "categories";
}

void RegisterWindow::loadProducts()
{
    if (!m_db) return;

    m_products.clear();

    QSqlQuery query(m_db->database());
    query.prepare(Queries::PRODUCT_SELECT_ALL);

    if (!query.exec()) {
        qWarning() << "Failed to load products:" << query.lastError().text();
        return;
    }

    while (query.next()) {
        QHash<QString, QVariant> product;
        product["id"] = query.value("id");
        product["name"] = query.value("name");
        product["price"] = query.value("price");
        product["cost"] = query.value("cost");
        product["stock"] = query.value("stock");
        product["category_id"] = query.value("category_id");
        product["category"] = query.value("category_name");
        product["image_path"] = query.value("image_path");
        product["is_service"] = query.value("is_service");
        product["has_variants"] = query.value("has_variants");

        m_products.append(product);
    }

    qDebug() << "Loaded" << m_products.size() << "products";
    displayProducts();
}

void RegisterWindow::displayProducts()
{
    if (!UIMap::itemsGridLayout) return;

    clearProductGrid();

    UIMap::itemsGridLayout->setSpacing(RegisterConstants::PRODUCT_GRID_SPACING);
    UIMap::itemsGridLayout->setContentsMargins(
        RegisterConstants::PRODUCT_GRID_MARGIN,
        RegisterConstants::PRODUCT_GRID_MARGIN,
        RegisterConstants::PRODUCT_GRID_MARGIN,
        RegisterConstants::PRODUCT_GRID_MARGIN);

    QList<QHash<QString, QVariant>> filtered = filterProducts();
    int columns = calculateColumnCount();

    int row = 0;
    int col = 0;

    for (const auto& product : filtered) {
        ProductButton* btn = ProductButtonCreator::createButton(product, m_db);

        connect(btn, &ProductButton::clicked, this, &RegisterWindow::onProductButtonClicked);

        UIMap::itemsGridLayout->addWidget(btn, row, col, Qt::AlignLeft | Qt::AlignTop);
        m_productButtons.append(btn);

        if (++col >= columns) {
            col = 0;
            row++;
        }
    }

    // No column stretch — buttons are fixed size, left-aligned
    // Only stretch the trailing empty row to push all content to the top
    UIMap::itemsGridLayout->setRowStretch(row + 1, 1);
}

int RegisterWindow::calculateColumnCount() const
{
    if (!UIMap::itemsGridLayout) return RegisterConstants::DEFAULT_COLUMNS;

    QWidget* parent = UIMap::itemsGridLayout->parentWidget();
    int availableWidth = 0;
    while (parent) {
        if (QScrollArea* scroll = qobject_cast<QScrollArea*>(parent->parentWidget())) {
            availableWidth = scroll->viewport()->width();
            break;
        }
        parent = parent->parentWidget();
    }

    if (availableWidth <= 0) {
        availableWidth = QApplication::primaryScreen()
        ? QApplication::primaryScreen()->availableGeometry().width() * 3 / 5
        : 800;
    }

    availableWidth -= (RegisterConstants::PRODUCT_GRID_MARGIN * 2);
    int cellSize = RegisterConstants::PRODUCT_BUTTON_SIZE + RegisterConstants::PRODUCT_GRID_SPACING;

    return qMax(RegisterConstants::MIN_COLUMNS, availableWidth / cellSize);
}

void RegisterWindow::clearProductGrid()
{
    if (!UIMap::itemsGridLayout) return;  // CHANGED

    QLayoutItem* item;
    while ((item = UIMap::itemsGridLayout->takeAt(0)) != nullptr) {  // CHANGED
        delete item->widget();
        delete item;
    }
    m_productButtons.clear();
}

QList<QHash<QString, QVariant>> RegisterWindow::filterProducts()
{
    // Read OOS visibility setting from settings.json.
    // Default true (show OOS items) if the key has never been written.
    bool showOOS = true;
    {
        const QString settingsPath = POSConfig::getAppDataPath() + POSConfig::SETTINGS_FILENAME;
        QFile sf(settingsPath);
        if (sf.open(QIODevice::ReadOnly)) {
            const QJsonObject obj = QJsonDocument::fromJson(sf.readAll()).object();
            sf.close();
            if (obj.contains("show_oos_products"))
                showOOS = obj.value("show_oos_products").toBool(true);
        }
    }

    QList<QHash<QString, QVariant>> filtered;

    for (const auto& product : m_products) {
        bool matchesSearch   = true;
        bool matchesCategory = true;
        bool matchesOOS      = true;

        // Search filter
        if (!m_currentSearchText.isEmpty()) {
            QString productName  = product.value("name").toString().toLower();
            QString categoryName = product.value("category").toString().toLower();
            QString searchLower  = m_currentSearchText.toLower();

            matchesSearch = productName.contains(searchLower) ||
                            categoryName.contains(searchLower);
        }

        // Category filter
        if (m_currentCategoryFilter != -1) {
            int productCategoryId = product.value("category_id").toInt();
            matchesCategory = (productCategoryId == m_currentCategoryFilter);
        }

        // OOS filter — only applies to tracked, non-service, non-variant products.
        // Services and variant products are always shown regardless of this setting
        // because they have no meaningful stock value.
        if (!showOOS) {
            const bool isService   = product.value("is_service").toBool();
            const bool hasVariants = product.value("has_variants").toBool();
            if (!isService && !hasVariants) {
                const int stock = product.value("stock").toInt();
                if (stock <= 0)
                    matchesOOS = false;
            }
        }

        if (matchesSearch && matchesCategory && matchesOOS) {
            filtered.append(product);
        }
    }

    return filtered;
}

// ===== RESIZE =====

void RegisterWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    displayProducts();
}

// ===== SEARCH & FILTER =====

void RegisterWindow::onSearchChanged(const QString& text)
{
    m_currentSearchText = text;
    m_searchTimer->start();
}

void RegisterWindow::onCategoryChanged(int index)
{
    if (!UIMap::filterBox) return;
    m_currentCategoryFilter = UIMap::filterBox->itemData(index).toInt();
    displayProducts();
}

void RegisterWindow::onSearchTimer()
{
    displayProducts();
}

// ===== PRODUCT ACTIONS =====

void RegisterWindow::onProductButtonClicked()
{
    QFrame* btn = qobject_cast<QFrame*>(sender());
    if (!btn) return;

    int  productId   = btn->property("productId").toInt();
    bool hasVariants = btn->property("hasVariants").toBool();

    if (hasVariants) {
        QString productName = btn->property("productName").toString();
        bool isService = btn->property("isService").toBool();

        VariantPickerDialog picker(productName, productId, m_db, isService, this);
        if (picker.exec() != QDialog::Accepted) return;

        addVariantToCart(productId, picker.selectedVariant());
    } else {
        addToCart(productId);
    }
}

void RegisterWindow::addToCart(int productId)
{
    // Find product in list
    QHash<QString, QVariant> product;
    for (const auto& p : m_products) {
        if (p.value("id").toInt() == productId) {
            product = p;
            break;
        }
    }

    if (product.isEmpty()) {
        qWarning() << "Product not found:" << productId;
        return;
    }

    QString name = product.value("name").toString();
    double price = product.value("price").toDouble();
    double cost = product.value("cost").toDouble();
    int stock = product.value("stock").toInt();
    bool isService = product.value("is_service").toBool();

    // Check stock
    if (!isService && stock <= 0) {
        AppHelpers::showWarning(this,
                                QString(tr("Product '%1' is out of stock.")).arg(name));
        return;
    }

    // Check if already in cart
    if (m_cart.contains(productId)) {
        if (!isService && m_cart[productId].quantity >= stock) {
            AppHelpers::showWarning(this,
                                    QString(tr(RegisterConstants::MSG_STOCK_EXCEEDED))
                                        .arg(name).arg(stock));
            return;
        }
        m_cart[productId].quantity += 1;
    } else {
        CartItem item;
        item.name = name;
        item.price = price;
        item.cost = cost;
        item.quantity = 1;
        item.stock = stock;
        item.isService = isService;
        item.originalProductId = productId;
        item.variantId = -1;

        m_cart.insert(productId, item);
    }

    updateCartDisplay();
    updateCartTotal();
    updateOpenTicketButton();
}

void RegisterWindow::addVariantToCart(int productId, const VariantOption& variant)
{
    // Cart key = variantId offset by VARIANT_KEY_OFFSET to avoid collision with product IDs
    int cartKey = variant.id + RegisterConstants::VARIANT_KEY_OFFSET;

    // Check is_service from parent product
    bool parentIsService = false;
    for (const auto& p : m_products) {
        if (p.value("id").toInt() == productId) {
            parentIsService = p.value("is_service").toBool();
            break;
        }
    }

    // Check stock for non-service variants
    if (!parentIsService && variant.stock <= 0) {
        AppHelpers::showWarning(this, QString(tr("Variant '%1' is out of stock.")).arg(variant.name));
        return;
    }

    if (m_cart.contains(cartKey)) {
        if (!parentIsService && m_cart[cartKey].quantity >= variant.stock) {
            AppHelpers::showWarning(this, QString(tr(RegisterConstants::MSG_STOCK_EXCEEDED))
                                        .arg(variant.name).arg(variant.stock));
            return;
        }
        m_cart[cartKey].quantity += 1;
    } else {
        CartItem item;
        item.name              = variant.name;
        item.price             = variant.price;
        item.cost              = variant.cost;
        item.quantity          = 1;
        item.stock             = variant.stock;
        item.isService         = parentIsService;
        item.originalProductId = productId;
        item.variantId         = variant.id;

        m_cart.insert(cartKey, item);
    }

    updateCartDisplay();
    updateCartTotal();
    updateOpenTicketButton();
}

// ===== CART MANAGEMENT =====

void RegisterWindow::updateCartDisplay()
{
    if (!UIMap::cartLayout) return;

    // Remove all cart item widgets, keeping the trailing stretch at the end
    while (UIMap::cartLayout->count() > 1) {
        QLayoutItem* item = UIMap::cartLayout->takeAt(0);
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    // Add cart items directly to the layout
    for (auto it = m_cart.constBegin(); it != m_cart.constEnd(); ++it) {
        int cartKey = it.key();
        const CartItem& item = it.value();

        int maxQty;
        if (item.isService || item.stock <= 0)
            maxQty = RegisterConstants::DEFAULT_MAX_QUANTITY;
        else
            maxQty = item.stock;

        CartItemWidget* widget = new CartItemWidget(
            cartKey, item.name, item.quantity, item.price, maxQty, this);

        connect(widget, &CartItemWidget::quantityChanged,
                this, &RegisterWindow::onCartQuantityChanged);
        connect(widget, &CartItemWidget::removeRequested,
                this, &RegisterWindow::onCartItemRemove);

        UIMap::cartLayout->insertWidget(UIMap::cartLayout->count() - 1, widget);
    }

    // Add separator + discount items if needed
    if (!m_discounts.isEmpty() && !m_cart.isEmpty()) {
        QWidget* separatorWidget = new QWidget();
        separatorWidget->setProperty("class", "discount-separator");

        QHBoxLayout* layout = new QHBoxLayout(separatorWidget);
        layout->setContentsMargins(10, 5, 10, 5);

        QLabel* label = new QLabel(tr("DISCOUNTS"));
        label->setProperty("class", "discount-separator-label");
        label->setAlignment(Qt::AlignCenter);
        layout->addWidget(label);

        UIMap::cartLayout->insertWidget(UIMap::cartLayout->count() - 1, separatorWidget);
    }

    double subtotal = 0.0;
    for (const auto& item : m_cart) {
        subtotal += item.price * item.quantity;
    }

    for (int i = 0; i < m_discounts.size(); ++i) {
        const DiscountItem& discount = m_discounts[i];
        double amount = DiscountHandler::calculateAmount(discount, subtotal);

        DiscountItemWidget* widget = new DiscountItemWidget(
            i, discount.name, discount.type, discount.value, amount, this);

        connect(widget, &DiscountItemWidget::removeRequested,
                this, &RegisterWindow::onDiscountRemove);

        UIMap::cartLayout->insertWidget(UIMap::cartLayout->count() - 1, widget);
    }
}

void RegisterWindow::updateCartTotal()
{
    // Calculate subtotal
    double subtotal = 0.0;
    for (const auto& item : m_cart) {
        subtotal += item.price * item.quantity;
    }

    // Calculate total discount
    double totalDiscount = 0.0;
    for (const DiscountItem& discount : m_discounts) {
        totalDiscount += DiscountHandler::calculateAmount(discount, subtotal);
    }

    // Calculate final total
    m_cartTotal = qMax(0.0, subtotal - totalDiscount);

    // Update label
    if (UIMap::totalLabel) {
        if (totalDiscount > 0) {
            UIMap::totalLabel->setText(
                QString(tr("Subtotal: %1\nDiscount: -%2\nTotal: %3"))
                    .arg(AppHelpers::formatPrice(subtotal))
                    .arg(AppHelpers::formatPrice(totalDiscount))
                    .arg(AppHelpers::formatPrice(m_cartTotal)));
        } else {
            UIMap::totalLabel->setText(
                QString(tr("Total: %1")).arg(AppHelpers::formatPrice(m_cartTotal)));
        }
    }
}

void RegisterWindow::onCartQuantityChanged(int cartKey, int newQuantity)
{
    if (m_cart.contains(cartKey)) {
        m_cart[cartKey].quantity = newQuantity;
        updateCartTotal();
    }
}

void RegisterWindow::onCartItemRemove(int cartKey)
{
    if (m_cart.contains(cartKey)) {
        m_cart.remove(cartKey);
        updateCartDisplay();
        updateCartTotal();
    }
}

void RegisterWindow::clearCart()
{
    m_cart.clear();
    m_discounts.clear();
    updateCartDisplay();
    updateCartTotal();
    updateOpenTicketButton();
}

// ===== TRANSACTION =====

bool RegisterWindow::validateTransaction()
{
    if (m_cart.isEmpty()) {
        AppHelpers::showWarning(this, tr(RegisterConstants::MSG_CART_EMPTY));
        return false;
    }

    // Check stock for all items
    for (const auto& item : m_cart) {
        if (item.isService) continue;

        // TODO: Get current stock from database
        // For now, assume stock is valid
    }

    return true;
}

void RegisterWindow::processTransaction()
{
    if (!validateTransaction()) return;

    if (!m_session) {
        AppHelpers::showError(this, tr(RegisterConstants::MSG_NO_USER_LOGGED_IN));
        return;
    }

    // Calculate totals
    double subtotal = 0.0;
    for (const auto& item : m_cart) {
        subtotal += item.price * item.quantity;
    }

    double totalDiscount = 0.0;
    for (const DiscountItem& discount : m_discounts) {
        totalDiscount += DiscountHandler::calculateAmount(discount, subtotal);
    }

    double total = qMax(0.0, subtotal - totalDiscount);

    // Generate ticket number
    QString ticketNumber = AppHelpers::generateTicketNumber();

    // Insert ticket
    QSqlQuery query(m_db->database());
    query.prepare(Queries::TICKET_INSERT);
    query.addBindValue(ticketNumber);
    query.addBindValue(QDate::currentDate());
    query.addBindValue(QTime::currentTime());
    query.addBindValue(m_session->username());
    query.addBindValue(m_session->displayName());
    query.addBindValue(subtotal);
    query.addBindValue(totalDiscount);
    query.addBindValue(0.0);  // tax_amount
    query.addBindValue(total);
    query.addBindValue("CASH");
    query.addBindValue("COMPLETED");
    query.addBindValue(QVariant());  // customer_name
    query.addBindValue(QVariant());  // customer_phone
    query.addBindValue(0.0);  // deposit_amount
    query.addBindValue(QVariant());  // description
    query.addBindValue(QVariant());  // due_date

    if (!query.exec()) {
        AppHelpers::showError(this,
                              QString(tr("%1\n\n%2"))
                                  .arg(tr(RegisterConstants::MSG_TRANSACTION_FAILED))
                                  .arg(query.lastError().text()));
        return;
    }

    int ticketId = query.lastInsertId().toInt();

    // Insert ticket items
    for (const auto& item : m_cart) {
        QSqlQuery itemQuery(m_db->database());
        itemQuery.prepare(Queries::TICKET_ITEM_INSERT);
        itemQuery.addBindValue(ticketId);
        itemQuery.addBindValue(item.originalProductId);
        itemQuery.addBindValue(QVariant());  // variant_id
        itemQuery.addBindValue(item.name);
        itemQuery.addBindValue(QVariant());  // variant_name
        itemQuery.addBindValue(QVariant());  // category_name
        itemQuery.addBindValue(QVariant());  // barcode
        itemQuery.addBindValue(item.quantity);
        itemQuery.addBindValue(item.price);
        itemQuery.addBindValue(item.cost);
        itemQuery.addBindValue(item.price * item.quantity);

        if (!itemQuery.exec()) {
            qWarning() << "Failed to insert ticket item:" << itemQuery.lastError().text();
        }
    }

    // Decrement stock for all sold items
    for (const auto& item : m_cart) {
        if (item.isService) continue;  // services and custom items have no stock

        if (item.variantId != -1) {
            // Variant item — decrement variant stock
            QSqlQuery stockQuery(m_db->database());
            stockQuery.prepare(Queries::VARIANT_UPDATE_STOCK);
            stockQuery.addBindValue(-item.quantity);
            stockQuery.addBindValue(item.variantId);
            if (!stockQuery.exec()) {
                qWarning() << "Failed to decrement variant stock for variant"
                           << item.variantId << ":" << stockQuery.lastError().text();
            }
            // Check if variant stock hit 0 and track out_of_stock_since
            updateVariantOutOfStock(item.variantId);
        } else if (item.originalProductId != -1) {
            // Regular product — decrement product stock
            QSqlQuery stockQuery(m_db->database());
            stockQuery.prepare(Queries::PRODUCT_UPDATE_STOCK);
            stockQuery.addBindValue(-item.quantity);
            stockQuery.addBindValue(item.originalProductId);
            if (!stockQuery.exec()) {
                qWarning() << "Failed to decrement product stock for product"
                           << item.originalProductId << ":" << stockQuery.lastError().text();
            }
            // Check if product stock hit 0 and track out_of_stock_since
            updateProductOutOfStock(item.originalProductId, item.variantId != -1);
        }
    }

    // Notify inventory app of stock changes
    if (m_notifier) {
        m_notifier->notifyChange(ChangeType::PRODUCT_CHANGED, 0);
        m_notifier->notifyChange(ChangeType::TICKET_CREATED, ticketId);
    }

    // Show success dialog with print option
    showTicketCompletedDialog(ticketId);

    // Clear cart and refresh
    clearCart();
    loadProducts();
}

// ===== BUTTON SLOTS =====

void RegisterWindow::onConfirmClicked()
{
    if (m_editMode)
        saveEditedTransaction();
    else
        processTransaction();
}

void RegisterWindow::onCancelClicked()
{
    if (m_editMode) {
        if (AppHelpers::confirmAction(this, tr("Cancel editing this ticket? All changes will be lost.")))
            exitEditMode();
        return;
    }

    if (m_cart.isEmpty() && m_discounts.isEmpty()) return;

    if (AppHelpers::confirmAction(this, tr(RegisterConstants::MSG_CANCEL_CONFIRM))) {
        clearCart();
    }
}

void RegisterWindow::onDiscountRemove(int discountIndex)
{
    if (discountIndex >= 0 && discountIndex < m_discounts.size()) {
        m_discounts.removeAt(discountIndex);
        updateCartDisplay();
        updateCartTotal();
    }
}

// ===== STUB METHODS (Coming Soon) =====

void RegisterWindow::onTicketsClicked()
{
    if (!m_session) return;

    if (TicketsLock::isRunning()) {
        // Tickets is already open — notify it to come to front
        if (m_notifier)
            m_notifier->notifyChange(ChangeType::TICKET_UPDATED, 0);
        return;
    }

    // Tickets not running — launch it
    QString executable = QCoreApplication::applicationDirPath() + "/../Tickets/Tickets";

    QStringList args;
    args << "--user" << m_session->username();

    if (!QProcess::startDetached(executable, args))
        AppHelpers::showError(this, tr("Failed to launch Tickets application."));
}

void RegisterWindow::onNotificationClicked()
{
    if (!m_notificationPanel) {
        QString username = m_session ? m_session->username() : "";
        m_notificationPanel = new NotificationPanel(m_notificationManager, username, m_db->database(), this);
    }

    if (m_notificationPanel->isVisible()) {
        m_notificationPanel->hide();
    } else {
        QPoint btnPos = UIMap::notificationBtn->mapToGlobal(QPoint(0, 0));
        m_notificationPanel->move(btnPos.x() - m_notificationPanel->width() + UIMap::notificationBtn->width(), 
                                 btnPos.y() + UIMap::notificationBtn->height());
        m_notificationPanel->show();
    }
}

void RegisterWindow::onDiscountClicked()
{
    if (m_cart.isEmpty()) {
        AppHelpers::showWarning(this, tr(RegisterConstants::MSG_CART_EMPTY));
        return;
    }

    double subtotal = 0.0;
    for (const auto& item : m_cart)
        subtotal += item.price * item.quantity;

    DiscountDialog dlg(m_db, subtotal, this);
    if (dlg.exec() != QDialog::Accepted) return;

    // Replace any existing discount — only one allowed
    m_discounts.clear();
    m_discounts.append(dlg.result());

    updateCartDisplay();
    updateCartTotal();
}

void RegisterWindow::onOpenTicketsClicked()
{
    // "Save Ticket" — cart has items
    if (!m_cart.isEmpty()) {
        saveAsOpenTicket();
        return;
    }

    // "Open Tickets" — cart is empty, show dialog
    OpenTicketsDialog dlg(m_db, this);
    if (dlg.exec() != QDialog::Accepted) return;

    const int ticketId = dlg.selectedTicketId();
    if (ticketId <= 0) return;

    if (dlg.selectedAction() == OpenTicketsDialog::Action::Complete) {
        completeOpenTicket(ticketId);
    } else if (dlg.selectedAction() == OpenTicketsDialog::Action::Edit) {
        enterOpenTicketEditMode(ticketId);
    } else if (dlg.selectedAction() == OpenTicketsDialog::Action::Print) {
        printTicket(ticketId);
    }
}

void RegisterWindow::onWithdrawClicked()
{
    if (m_session && !AccessControl::canPerformAction(
            m_session->username(), AccessControl::WithdrawCash, m_db->database())) {
        AppHelpers::showWarning(this, tr("You do not have permission to withdraw cash."));
        return;
    }

    QInputDialog dlg(this);
    dlg.setWindowTitle(tr("Withdraw"));
    dlg.setLabelText(tr("Enter withdrawal amount:"));
    dlg.setInputMode(QInputDialog::DoubleInput);
    dlg.setDoubleMinimum(0.01);
    dlg.setDoubleMaximum(999999.0);
    dlg.setDoubleDecimals(2);
    dlg.setOkButtonText(tr("Withdraw"));
    dlg.setCancelButtonText(tr("Cancel"));

    if (dlg.exec() != QDialog::Accepted) return;

    const double amount = dlg.doubleValue();
    if (amount <= 0) return;

    const QString userName = m_session ? m_session->displayName() : tr("Unknown");

    QSqlQuery ins(m_db->database());
    ins.prepare(Queries::ONE_TIME_EXPENSE_INSERT);
    ins.addBindValue(userName);
    ins.addBindValue(amount);
    ins.addBindValue(QDate::currentDate().toString("yyyy-MM-dd"));
    ins.addBindValue(tr("Withdrawal from Register"));

    if (!ins.exec()) {
        AppHelpers::showError(this, tr("Failed to record withdrawal.\n\n") + ins.lastError().text());
        return;
    }

    AppHelpers::showInfo(this, QString(tr("Withdrawal of %1 recorded successfully.")).arg(AppHelpers::formatPrice(amount)));
}

void RegisterWindow::onCustomItemClicked()
{
    CustomItemDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) return;

    CartItem item;
    item.name              = dlg.itemName();
    item.price             = dlg.itemPrice();
    item.cost              = dlg.itemCost();
    item.quantity          = 1;
    item.isService         = true;   // no stock tracking for custom items
    item.originalProductId = -1;
    item.variantId         = -1;

    m_cart.insert(m_customItemCounter--, item);

    updateCartDisplay();
    updateCartTotal();
}

void RegisterWindow::updateNotificationBadge()
{
    if (!UIMap::notificationBtn) return;

    int count = m_notificationManager ? m_notificationManager->unreadCount() : 0;

    if (count > 0) {
        UIMap::notificationBtn->setIcon(QIcon(StyleManager::iconPath("notification")));
        UIMap::notificationBtn->setStyleSheet(
            "QPushButton { background-color: #4A90E2; border-radius: 18px; }");
    } else {
        UIMap::notificationBtn->setIcon(QIcon(StyleManager::iconPath("notification")));
        UIMap::notificationBtn->setStyleSheet("");
    }
}

void RegisterWindow::onLogoutClicked()
{
    if (!LogoutManager::confirmAndLogout(this, m_session->sessionId(), m_session->username())) {
        return;
    }
    close();
    emit logoutRequested();
}

// ===== EDIT MODE =====

void RegisterWindow::checkPendingEditRequest()
{
    int ticketId = TicketEditRequest::read();
    if (ticketId <= 0) return;

    // Bring window to front reliably
    showNormal();
    raise();
    activateWindow();
    QWidget::activateWindow();

    // Warn if cart is not empty
    if (!m_cart.isEmpty()) {
        if (!AppHelpers::confirmAction(this,
                                       tr("There are items in the current cart.\n"
                                       "Loading the ticket to edit will clear the cart. Continue?")))
            return;
    }

    TicketEditRequest::clear();
    enterEditMode(ticketId);
}

void RegisterWindow::enterEditMode(int ticketId)
{
    if (!m_db) return;

    clearCart();

    // Load ticket items from DB into cart
    QSqlQuery query(m_db->database());
    query.prepare(Queries::TICKET_ITEM_SELECT_BY_TICKET);
    query.addBindValue(ticketId);

    if (!query.exec()) {
        qWarning() << "enterEditMode: failed to load ticket items:" << query.lastError().text();
        AppHelpers::showError(this, tr("Failed to load ticket items for editing."));
        return;
    }

    while (query.next()) {
        int     productId  = query.value("product_id").toInt();
        int     variantId  = query.value("variant_id").toInt();
        QString name       = query.value("product_name").toString();
        QString varName    = query.value("variant_name").toString();
        int     qty        = query.value("quantity").toInt();
        double  unitPrice  = query.value("unit_price").toDouble();
        double  unitCost   = query.value("unit_cost").toDouble();

        CartItem item;
        item.name              = varName.isEmpty() ? name : QString(tr("%1 — %2")).arg(name, varName);
        item.price             = unitPrice;
        item.cost              = unitCost;
        item.quantity          = qty;
        item.stock             = 0;    // not enforced in edit mode
        item.isService         = (productId <= 0 && variantId <= 0);
        item.originalProductId = productId > 0 ? productId : -1;
        item.variantId         = variantId > 0 ? variantId : -1;

        // Cart key: variantId > 0 → use variantId, productId > 0 → use productId, else custom counter
        int cartKey;
        if (variantId > 0)
            cartKey = variantId;
        else if (productId > 0)
            cartKey = productId;
        else
            cartKey = m_customItemCounter--;

        m_cart.insert(cartKey, item);
    }

    m_editMode     = true;
    m_editTicketId = ticketId;

    updateCartDisplay();
    updateCartTotal();

    // Update UI to reflect edit mode
    UIMap::confirmBtn->setText(tr("Save Changes"));
    UIMap::cancelBtn->setText(tr("Cancel Edit"));
    setWindowTitle(tr("POSBox - Register  [EDITING TICKET]"));

    qDebug() << "Entered edit mode for ticket" << ticketId;
}

void RegisterWindow::exitEditMode()
{
    m_editMode     = false;
    m_editTicketId = -1;

    // Also reset open ticket edit state if active
    m_openTicketEditMode = false;
    m_openTicketEditId   = -1;
    m_openTicketCustomerName.clear();
    m_openTicketAdvance  = 0.0;
    m_openTicketDescription.clear();

    clearCart();

    UIMap::confirmBtn->setText(tr("Confirm Sale"));
    UIMap::cancelBtn->setText(tr("Cancel"));
    setWindowTitle(tr("POSBox - Register"));

    qDebug() << "Exited edit mode";
}

void RegisterWindow::saveEditedTransaction()
{
    if (!validateTransaction()) return;
    if (!m_db || m_editTicketId <= 0) return;

    // ── Snapshot old items for stock diff ────────────────────────────────────
    QHash<int, int> oldProductQtys;
    QHash<int, int> oldVariantQtys;

    QSqlQuery oldQuery(m_db->database());
    oldQuery.prepare(Queries::TICKET_ITEM_SELECT_BY_TICKET);
    oldQuery.addBindValue(m_editTicketId);

    if (oldQuery.exec()) {
        while (oldQuery.next()) {
            int productId = oldQuery.value("product_id").toInt();
            int variantId = oldQuery.value("variant_id").toInt();
            int qty       = oldQuery.value("quantity").toInt();
            if (variantId > 0) {
                oldVariantQtys[variantId] += qty;
            } else if (productId > 0) {
                oldProductQtys[productId] += qty;
            }
        }
    }

    // ── Recalculate totals ────────────────────────────────────────────────────
    double subtotal = 0.0;
    for (const auto& item : m_cart)
        subtotal += item.price * item.quantity;

    double totalDiscount = 0.0;
    for (const DiscountItem& discount : m_discounts)
        totalDiscount += DiscountHandler::calculateAmount(discount, subtotal);

    double total = qMax(0.0, subtotal - totalDiscount);

    // ── Update ticket record ──────────────────────────────────────────────────
    QSqlQuery upd(m_db->database());
    upd.prepare(Queries::TICKET_UPDATE_STATUS);   // reuse: UPDATE tickets SET status=? WHERE id=?
    // We need a fuller update — use a raw query for the totals
    upd.prepare("UPDATE tickets SET subtotal=?, discount_amount=?, total=? WHERE id=?");
    upd.addBindValue(subtotal);
    upd.addBindValue(totalDiscount);
    upd.addBindValue(total);
    upd.addBindValue(m_editTicketId);

    if (!upd.exec()) {
        AppHelpers::showError(this, tr("Failed to update ticket totals.\n\n") + upd.lastError().text());
        return;
    }

    // ── Replace ticket items ──────────────────────────────────────────────────
    QSqlQuery del(m_db->database());
    del.prepare(Queries::TICKET_ITEM_DELETE_BY_TICKET);
    del.addBindValue(m_editTicketId);
    if (!del.exec()) {
        AppHelpers::showError(this, tr("Failed to clear old ticket items.\n\n") + del.lastError().text());
        return;
    }

    for (const auto& item : m_cart) {
        QSqlQuery ins(m_db->database());
        ins.prepare(Queries::TICKET_ITEM_INSERT);
        ins.addBindValue(m_editTicketId);
        ins.addBindValue(item.originalProductId > 0 ? item.originalProductId : QVariant());
        ins.addBindValue(item.variantId > 0 ? item.variantId : QVariant());
        ins.addBindValue(item.name);
        ins.addBindValue(QVariant());   // variant_name (already merged into name)
        ins.addBindValue(QVariant());   // category_name
        ins.addBindValue(QVariant());   // barcode
        ins.addBindValue(item.quantity);
        ins.addBindValue(item.price);
        ins.addBindValue(item.cost);
        ins.addBindValue(item.price * item.quantity);

        if (!ins.exec())
            qWarning() << "saveEditedTransaction: failed to insert item:" << ins.lastError().text();
    }

    // ── Snapshot new items for stock diff ────────────────────────────────────
    QHash<int, int> newProductQtys;
    QHash<int, int> newVariantQtys;
    for (const auto& item : m_cart) {
        if (item.variantId > 0 && !item.isService) {
            newVariantQtys[item.variantId] += item.quantity;
        } else if (item.originalProductId > 0 && item.variantId <= 0 && !item.isService) {
            newProductQtys[item.originalProductId] += item.quantity;
        }
    }

    // ── Adjust stock based on diff ────────────────────────────────────────────
    adjustStockDiff(oldProductQtys, newProductQtys);
    adjustStockDiff(oldVariantQtys, newVariantQtys, true);

    // ── Notify other apps ─────────────────────────────────────────────────────
    if (m_notifier) {
        m_notifier->notifyChange(ChangeType::PRODUCT_CHANGED, 0);
        m_notifier->notifyChange(ChangeType::TICKET_UPDATED, m_editTicketId);
    }

    AppHelpers::showInfo(this, tr("Ticket updated successfully."));

    exitEditMode();
    loadProducts();
}

void RegisterWindow::adjustStockDiff(const QHash<int, int>& oldQtys,
                                     const QHash<int, int>& newQtys)
{
    // Collect all product IDs from both snapshots
    QSet<int> allIds;
    for (auto it = oldQtys.begin(); it != oldQtys.end(); ++it) allIds.insert(it.key());
    for (auto it = newQtys.begin(); it != newQtys.end(); ++it) allIds.insert(it.key());

    for (int productId : allIds) {
        int oldQty = oldQtys.value(productId, 0);
        int newQty = newQtys.value(productId, 0);
        int diff   = newQty - oldQty;  // positive = need to deduct more, negative = restore

        if (diff == 0) continue;

        // Check if product is a service before touching stock
        QSqlQuery chk(m_db->database());
        chk.prepare("SELECT is_service FROM products WHERE id = ?");
        chk.addBindValue(productId);
        if (!chk.exec() || !chk.next()) continue;
        if (chk.value("is_service").toBool()) continue;

        QSqlQuery sq(m_db->database());
        sq.prepare(Queries::PRODUCT_UPDATE_STOCK);
        sq.addBindValue(-diff);   // negative diff means restore (add back), positive means deduct
        sq.addBindValue(productId);

        if (!sq.exec())
            qWarning() << "adjustStockDiff: failed for product" << productId << sq.lastError().text();
    }
}

void RegisterWindow::adjustStockDiff(const QHash<int, int>& oldQtys,
                                     const QHash<int, int>& newQtys,
                                     bool isVariant)
{
    Q_UNUSED(isVariant);
    QSet<int> allIds;
    for (auto it = oldQtys.begin(); it != oldQtys.end(); ++it) allIds.insert(it.key());
    for (auto it = newQtys.begin(); it != newQtys.end(); ++it) allIds.insert(it.key());

    for (int variantId : allIds) {
        int oldQty = oldQtys.value(variantId, 0);
        int newQty = newQtys.value(variantId, 0);
        int diff   = newQty - oldQty;

        if (diff == 0) continue;

        QSqlQuery sq(m_db->database());
        sq.prepare(Queries::VARIANT_UPDATE_STOCK);
        sq.addBindValue(-diff);
        sq.addBindValue(variantId);

        if (!sq.exec())
            qWarning() << "adjustStockDiff: failed for variant" << variantId << sq.lastError().text();
    }
}
// ===== OPEN TICKET BUTTON STATE =====

void RegisterWindow::updateOpenTicketButton()
{
    if (!UIMap::openTicketsBtn) return;

    // If cart has items (in any mode) → "Save Ticket"
    // Otherwise → "Open Tickets"
    if (!m_cart.isEmpty()) {
        UIMap::openTicketsBtn->setText(tr("Save Ticket"));
    } else {
        UIMap::openTicketsBtn->setText(tr("Open Tickets"));
    }
}

// ===== OPEN TICKET OPERATIONS =====

void RegisterWindow::saveAsOpenTicket()
{
    if (!validateTransaction()) return;
    if (!m_session || !m_db) return;

    // If we're editing an existing open ticket, pre-fill the dialog
    SaveTicketDialog dlg(this);
    if (m_openTicketEditMode) {
        dlg.setCustomerName(m_openTicketCustomerName);
        dlg.setAdvancePayment(m_openTicketAdvance);
        dlg.setDescription(m_openTicketDescription);
    }

    if (dlg.exec() != QDialog::Accepted) return;

    const QString customerName = dlg.customerName();
    const double  advance      = dlg.advancePayment();
    const QString description  = dlg.description();

    // Calculate totals
    double subtotal = 0.0;
    for (const auto& item : m_cart)
        subtotal += item.price * item.quantity;

    double totalDiscount = 0.0;
    for (const DiscountItem& discount : m_discounts)
        totalDiscount += DiscountHandler::calculateAmount(discount, subtotal);

    const double total = qMax(0.0, subtotal - totalDiscount);

    if (m_openTicketEditMode) {
        // ── Overwrite existing open ticket ───────────────────────────────────
        QSqlQuery upd(m_db->database());
        upd.prepare(
            "UPDATE tickets SET "
            "subtotal=?, discount_amount=?, total=?, "
            "customer_name=?, deposit_amount=?, description=? "
            "WHERE id=?");
        upd.addBindValue(subtotal);
        upd.addBindValue(totalDiscount);
        upd.addBindValue(total);
        upd.addBindValue(customerName.isEmpty() ? QVariant() : customerName);
        upd.addBindValue(advance);
        upd.addBindValue(description.isEmpty() ? QVariant() : description);
        upd.addBindValue(m_openTicketEditId);

        if (!upd.exec()) {
            AppHelpers::showError(this, tr("Failed to update ticket.\n\n") + upd.lastError().text());
            return;
        }

        // Replace items
        QSqlQuery del(m_db->database());
        del.prepare(Queries::TICKET_ITEM_DELETE_BY_TICKET);
        del.addBindValue(m_openTicketEditId);
        del.exec();

        for (const auto& item : m_cart) {
            QSqlQuery ins(m_db->database());
            ins.prepare(Queries::TICKET_ITEM_INSERT);
            ins.addBindValue(m_openTicketEditId);
            ins.addBindValue(item.originalProductId > 0 ? item.originalProductId : QVariant());
            ins.addBindValue(item.variantId > 0 ? item.variantId : QVariant());
            ins.addBindValue(item.name);
            ins.addBindValue(QVariant()); // variant_name
            ins.addBindValue(QVariant()); // category_name
            ins.addBindValue(QVariant()); // barcode
            ins.addBindValue(item.quantity);
            ins.addBindValue(item.price);
            ins.addBindValue(item.cost);
            ins.addBindValue(item.price * item.quantity);
            if (!ins.exec())
                qWarning() << "saveAsOpenTicket: failed to insert item:" << ins.lastError().text();
        }

        if (m_notifier)
            m_notifier->notifyChange(ChangeType::TICKET_UPDATED, m_openTicketEditId);

        AppHelpers::showInfo(this, tr("Open ticket updated successfully."));

        // Exit open ticket edit mode
        m_openTicketEditMode = false;
        m_openTicketEditId   = -1;
        m_openTicketCustomerName.clear();
        m_openTicketAdvance  = 0.0;
        m_openTicketDescription.clear();

    } else {
        // ── Create new open ticket ────────────────────────────────────────────
        const QString ticketNumber = AppHelpers::generateTicketNumber();

        QSqlQuery query(m_db->database());
        query.prepare(Queries::TICKET_INSERT);
        query.addBindValue(ticketNumber);
        query.addBindValue(QDate::currentDate());
        query.addBindValue(QTime::currentTime());
        query.addBindValue(m_session->username());
        query.addBindValue(m_session->displayName());
        query.addBindValue(subtotal);
        query.addBindValue(totalDiscount);
        query.addBindValue(0.0);   // tax_amount
        query.addBindValue(total);
        query.addBindValue("CASH");
        query.addBindValue("OPEN");
        query.addBindValue(customerName.isEmpty() ? QVariant() : customerName);
        query.addBindValue(QVariant()); // customer_phone
        query.addBindValue(advance);
        query.addBindValue(description.isEmpty() ? QVariant() : description);
        query.addBindValue(QVariant()); // due_date

        if (!query.exec()) {
            AppHelpers::showError(this, tr("Failed to save ticket.\n\n") + query.lastError().text());
            return;
        }

        const int ticketId = query.lastInsertId().toInt();

        for (const auto& item : m_cart) {
            QSqlQuery ins(m_db->database());
            ins.prepare(Queries::TICKET_ITEM_INSERT);
            ins.addBindValue(ticketId);
            ins.addBindValue(item.originalProductId > 0 ? item.originalProductId : QVariant());
            ins.addBindValue(item.variantId > 0 ? item.variantId : QVariant());
            ins.addBindValue(item.name);
            ins.addBindValue(QVariant());
            ins.addBindValue(QVariant());
            ins.addBindValue(QVariant());
            ins.addBindValue(item.quantity);
            ins.addBindValue(item.price);
            ins.addBindValue(item.cost);
            ins.addBindValue(item.price * item.quantity);
            if (!ins.exec())
                qWarning() << "saveAsOpenTicket: failed to insert item:" << ins.lastError().text();
        }

        if (m_notifier)
            m_notifier->notifyChange(ChangeType::TICKET_CREATED, ticketId);

        AppHelpers::showInfo(this, QString(tr("Ticket #%1 saved as open.")).arg(ticketNumber));
    }

    clearCart();
    loadProducts();
}

void RegisterWindow::completeOpenTicket(int ticketId)
{
    if (!m_db || ticketId <= 0) return;

    // ── Check stock before doing anything ────────────────────────────────────
    QSqlQuery itemQuery(m_db->database());
    itemQuery.prepare(Queries::TICKET_ITEM_SELECT_BY_TICKET);
    itemQuery.addBindValue(ticketId);

    if (!itemQuery.exec()) {
        AppHelpers::showError(this, tr("Failed to load ticket items."));
        return;
    }

    QStringList insufficientItems;

    while (itemQuery.next()) {
        const int    productId = itemQuery.value("product_id").toInt();
        const int    variantId = itemQuery.value("variant_id").toInt();
        const int    qty       = itemQuery.value("quantity").toInt();
        const QString name     = itemQuery.value("product_name").toString();

        if (variantId > 0) {
            // Join with parent product to check is_service before checking stock
            QSqlQuery chk(m_db->database());
            chk.prepare(
                "SELECT pv.stock, p.is_service "
                "FROM product_variants pv "
                "JOIN products p ON pv.parent_product_id = p.id "
                "WHERE pv.id = ?");
            chk.addBindValue(variantId);
            if (chk.exec() && chk.next()) {
                if (!chk.value("is_service").toBool() &&
                    chk.value("stock").toInt() < qty)
                    insufficientItems << QString(tr("- %1 (need %2)")).arg(name).arg(qty);
            }
        } else if (productId > 0) {
            QSqlQuery chk(m_db->database());
            chk.prepare("SELECT is_service, stock_quantity FROM products WHERE id = ?");
            chk.addBindValue(productId);
            if (chk.exec() && chk.next()) {
                if (!chk.value("is_service").toBool() &&
                    chk.value("stock_quantity").toInt() < qty)
                    insufficientItems << QString(tr("- %1 (need %2)")).arg(name).arg(qty);
            }
        }
    }

    if (!insufficientItems.isEmpty()) {
        AppHelpers::showWarning(this,
                                tr("The following items no longer have enough stock:\n\n") +
                                    insufficientItems.join("\n") +
                                    tr("\n\nPlease edit the ticket before completing."));
        return;
    }

    // ── All stock OK — mark as completed and deduct ───────────────────────────
    QSqlQuery upd(m_db->database());
    upd.prepare(Queries::TICKET_UPDATE_STATUS);
    upd.addBindValue("COMPLETED");
    upd.addBindValue(QDate::currentDate().toString("yyyy-MM-dd"));
    upd.addBindValue(ticketId);

    if (!upd.exec()) {
        AppHelpers::showError(this, tr("Failed to complete ticket.\n\n") + upd.lastError().text());
        return;
    }

    deductStockForTicket(ticketId);

    if (m_notifier) {
        m_notifier->notifyChange(ChangeType::PRODUCT_CHANGED, 0);
        m_notifier->notifyChange(ChangeType::TICKET_UPDATED, ticketId);
    }

    showTicketCompletedDialog(ticketId);
    loadProducts();
}

void RegisterWindow::deductStockForTicket(int ticketId)
{
    if (!m_db || ticketId <= 0) return;

    QSqlQuery query(m_db->database());
    query.prepare(Queries::TICKET_ITEM_SELECT_BY_TICKET);
    query.addBindValue(ticketId);

    if (!query.exec()) {
        qWarning() << "deductStockForTicket: failed to load items:" << query.lastError().text();
        return;
    }

    while (query.next()) {
        const int    productId = query.value("product_id").toInt();
        const int    variantId = query.value("variant_id").toInt();
        const int    qty       = query.value("quantity").toInt();

        if (variantId > 0) {
            QSqlQuery sq(m_db->database());
            sq.prepare(Queries::VARIANT_UPDATE_STOCK);
            sq.addBindValue(-qty);
            sq.addBindValue(variantId);
            if (!sq.exec())
                qWarning() << "deductStockForTicket: variant stock failed:" << sq.lastError().text();
            updateVariantOutOfStock(variantId);
        } else if (productId > 0) {
            // Skip services
            QSqlQuery chk(m_db->database());
            chk.prepare("SELECT is_service FROM products WHERE id = ?");
            chk.addBindValue(productId);
            if (chk.exec() && chk.next() && chk.value(0).toBool()) continue;

            QSqlQuery sq(m_db->database());
            sq.prepare(Queries::PRODUCT_UPDATE_STOCK);
            sq.addBindValue(-qty);
            sq.addBindValue(productId);
            if (!sq.exec())
                qWarning() << "deductStockForTicket: product stock failed:" << sq.lastError().text();
            updateProductOutOfStock(productId, false);
        }
    }
}

void RegisterWindow::enterOpenTicketEditMode(int ticketId)
{
    if (!m_db || ticketId <= 0) return;

    if (!m_cart.isEmpty()) {
        if (!AppHelpers::confirmAction(this,
                                       tr("There are items in the current cart.\n"
                                       "Loading the ticket to edit will clear the cart. Continue?")))
            return;
    }

    // Load ticket metadata
    QSqlQuery meta(m_db->database());
    meta.prepare(Queries::TICKET_SELECT_BY_ID);
    meta.addBindValue(ticketId);

    if (!meta.exec() || !meta.next()) {
        AppHelpers::showError(this, tr("Failed to load ticket details."));
        return;
    }

    m_openTicketCustomerName = meta.value("customer_name").toString();
    m_openTicketAdvance      = meta.value("deposit_amount").toDouble();
    m_openTicketDescription  = meta.value("description").toString();

    // Reuse existing enterEditMode to load items into cart
    enterEditMode(ticketId);

    // Override the edit mode flags — this is an open ticket edit, not a completed one
    m_editMode           = false;
    m_editTicketId       = -1;
    m_openTicketEditMode = true;
    m_openTicketEditId   = ticketId;

    // Update UI
    UIMap::confirmBtn->setText(tr("Confirm Sale"));
    UIMap::cancelBtn->setText(tr("Cancel"));
    setWindowTitle(tr("POSBox - Register  [EDITING OPEN TICKET]"));
    updateOpenTicketButton();
}

// ===== TICKET COMPLETED DIALOG =====

void RegisterWindow::showTicketCompletedDialog(int ticketId)
{
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("Ticket Completed"));
    msgBox.setText(tr("Ticket completed and stock updated."));
    msgBox.setInformativeText(tr("Would you like to print this ticket?"));

    QPushButton* printBtn = new QPushButton(tr("Print Ticket"), &msgBox);
    QPushButton* closeBtn = new QPushButton(tr("Close"), &msgBox);
    printBtn->setObjectName("btnStyle-add");
    closeBtn->setObjectName("btnStyle-delete");

    msgBox.addButton(closeBtn, QMessageBox::RejectRole);
    msgBox.addButton(printBtn, QMessageBox::AcceptRole);
    msgBox.setDefaultButton(printBtn);

    printBtn->style()->unpolish(printBtn);
    printBtn->style()->polish(printBtn);
    closeBtn->style()->unpolish(closeBtn);
    closeBtn->style()->polish(closeBtn);

    msgBox.exec();

    if (msgBox.clickedButton() == printBtn) {
        printTicket(ticketId);
    }
}

void RegisterWindow::printTicket(int ticketId)
{
    if (!m_db || ticketId <= 0) return;

    // Fetch shop info
    ThermalPrinter::ShopInfo shop;
    {
        QSqlQuery q(m_db->database());
        q.prepare(Queries::PROFILE_SELECT);
        if (q.exec() && q.next()) {
            shop.name     = q.value("shop_name").toString();
            shop.phone    = q.value("phone").toString();
            shop.email    = q.value("email").toString();
            shop.logoPath = q.value("logo_path").toString();
        }
    }

    // Fetch ticket totals
    double subtotal       = 0.0;
    double discountAmount = 0.0;
    double total          = 0.0;
    QString status;
    {
        QSqlQuery q(m_db->database());
        q.prepare(Queries::TICKET_SELECT_BY_ID);
        q.addBindValue(ticketId);
        if (q.exec() && q.next()) {
            subtotal        = q.value("subtotal").toDouble();
            discountAmount = q.value("discount_amount").toDouble();
            total         = q.value("total").toDouble();
            status        = q.value("status").toString();
        }
    }

    // Fetch ticket items
    QList<ThermalPrinter::TicketItem> items;
    {
        QSqlQuery q(m_db->database());
        q.prepare(Queries::TICKET_ITEM_SELECT_BY_TICKET);
        q.addBindValue(ticketId);
        if (q.exec()) {
            while (q.next()) {
                ThermalPrinter::TicketItem item;
                const QString name = q.value("product_name").toString();
                const QString variant = q.value("variant_name").toString();
                item.name       = variant.isEmpty() ? name : QString(tr("%1 - %2")).arg(name, variant);
                item.quantity   = q.value("quantity").toInt();
                item.unitPrice  = q.value("unit_price").toDouble();
                items << item;
            }
        }
    }

    ThermalPrinter::TicketData data;
    data.shop           = shop;
    data.items          = items;
    data.subtotal       = subtotal;
    data.discountAmount = discountAmount;
    data.total          = total;
    data.isUnpaid       = (status == "OPEN");

    ThermalPrinter printer(m_db->database());
    if (!printer.print(data)) {
        AppHelpers::showError(this, tr("Failed to print ticket.\n\n") + printer.lastError());
    }
}

// ===== OUT OF STOCK TRACKING =====

void RegisterWindow::updateProductOutOfStock(int productId, bool hasVariants)
{
    if (!m_db || productId <= 0) return;

    if (hasVariants) {
        QSqlQuery check(m_db->database());
        check.prepare(Queries::VARIANT_SELECT_BY_PRODUCT);
        check.addBindValue(productId);
        if (!check.exec()) return;

        bool allOutOfStock = true;
        while (check.next()) {
            if (check.value("stock").toInt() > 0) {
                allOutOfStock = false;
                break;
            }
        }

        QSqlQuery update(m_db->database());
        if (allOutOfStock) {
            update.prepare(Queries::OUT_OF_STOCK_SET);
            update.addBindValue(productId);
            update.exec();
        } else {
            update.prepare(Queries::OUT_OF_STOCK_CLEAR);
            update.addBindValue(productId);
            update.exec();
        }
    } else {
        QSqlQuery check(m_db->database());
        check.prepare("SELECT stock_quantity FROM products WHERE id = ?");
        check.addBindValue(productId);
        if (check.exec() && check.next()) {
            int stock = check.value("stock_quantity").toInt();
            QSqlQuery update(m_db->database());
            if (stock <= 0) {
                update.prepare(Queries::OUT_OF_STOCK_SET);
                update.addBindValue(productId);
            } else {
                update.prepare(Queries::OUT_OF_STOCK_CLEAR);
                update.addBindValue(productId);
            }
            update.exec();
        }
    }
}

void RegisterWindow::updateVariantOutOfStock(int variantId)
{
    if (!m_db || variantId <= 0) return;

    QSqlQuery check(m_db->database());
    check.prepare("SELECT parent_product_id, stock FROM product_variants WHERE id = ?");
    check.addBindValue(variantId);
    if (!check.exec() || !check.next()) return;

    int parentId = check.value("parent_product_id").toInt();
    int stock = check.value("stock").toInt();

    QSqlQuery parentCheck(m_db->database());
    parentCheck.prepare("SELECT has_variants FROM products WHERE id = ?");
    parentCheck.addBindValue(parentId);
    if (!parentCheck.exec() || !parentCheck.next()) return;

    bool hasVariants = parentCheck.value("has_variants").toBool();
    if (hasVariants) {
        updateProductOutOfStock(parentId, true);
    } else {
        QSqlQuery update(m_db->database());
        if (stock <= 0) {
            update.prepare(Queries::OUT_OF_STOCK_VARIANT_SET);
            update.addBindValue(variantId);
        } else {
            update.prepare(Queries::OUT_OF_STOCK_VARIANT_CLEAR);
            update.addBindValue(variantId);
        }
        update.exec();
    }
}
