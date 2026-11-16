#include "RegisterWindow.h"
#include "MessageDialog.h"
#include "ModernDialog.h"
#include "Animations.h"
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
#include "LocalApi.h"
#include "LocalImages.h"
#include "LocalSettings.h"
#include "Permissions.h"
#include "ChangeNotifier.h"
#include "SessionManager.h"
#include "StyleManager.h"
#include "AccessControl.h"
#include "LogoutManager.h"
#include "NotificationActions.h"
#include "NotificationManager.h"
#include "NotificationBell.h"
#include "ThermalPrinter.h"
#include <QVBoxLayout>
#include <QTimer>
#include <QScrollArea>
#include <QFrame>
#include <QSet>
#include <QApplication>
#include <QScreen>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <algorithm>

namespace {
constexpr int ConfirmTextMargin = 12;
}

RegisterWindow::RegisterWindow(UserSession* session, LocalApi* api,
                               LocalImages* images,
                               ChangeNotifier* notifier, QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::RegisterWindow)
    , m_session(session)
    , m_api(api)
    , m_images(images)
    , m_notifier(notifier)
    , m_resizeRebuildTimer(nullptr)
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
{
    ui->setupUi(this);
    m_confirmText = tr("Confirm");

    // Apply global styles
    StyleManager::applySavedTheme(this);

    // Initialize UI mapping
    UIMap::initialize(ui);
    buildLayout();

    // Setup notification system
    m_bell = new NotificationBell(m_api, UIMap::notificationBtn,
                                  m_session ? m_session->username() : QString(),
                                  m_notifier, this);


    // Bounded staleness: refresh income directly on every tick, not only
    // when notifications_sync happens to succeed and emit. A dead push path
    // then costs at most one interval instead of "until my next own sale".
    connect(m_bell->refreshTimer(), &QTimer::timeout, this, [this]() { updateDailyIncome(); });
    connect(m_bell->manager(), &NotificationManager::notificationsChanged,
            this, [this]() { updateDailyIncome(); });

    // Setup
    setupUI();
    setupConnections();

    // Load data
    loadCategories();
    loadProducts();
    updateDailyIncome();
    updateCartTotal();

    // Show maximized
    showMaximized();
}

RegisterWindow::~RegisterWindow()
{
    delete ui;
}

void RegisterWindow::setupUI()
{
    setWindowTitle(tr("POSBox - Register"));

    // Configure search timer
    m_searchTimer->setSingleShot(true);
    m_searchTimer->setInterval(AppConstants::SEARCH_DELAY_MS);

    // Debounced grid rebuild: resize events arrive in a storm while the
    // window is dragged; rebuild once the size settles.
    m_resizeRebuildTimer = new QTimer(this);
    m_resizeRebuildTimer->setSingleShot(true);
    m_resizeRebuildTimer->setInterval(150);
    connect(m_resizeRebuildTimer, &QTimer::timeout, this, &RegisterWindow::displayProducts);

    // Update user display
    updateUserDisplay();
}

void RegisterWindow::setupConnections()
{
    // Top bar
    connect(UIMap::notificationBtn, &QPushButton::clicked, m_bell, &NotificationBell::togglePanel);
    connect(UIMap::logoutBtn, &QPushButton::clicked, this, &RegisterWindow::onLogoutClicked);

    // Products section
    connect(UIMap::searchBar, &QLineEdit::textChanged, this, &RegisterWindow::onSearchChanged);
    connect(UIMap::filterBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RegisterWindow::onCategoryChanged);
    connect(UIMap::customItemBtn, &QPushButton::clicked, this, &RegisterWindow::onCustomItemClicked);
    connect(m_searchTimer, &QTimer::timeout, this, &RegisterWindow::onSearchTimer);

    // Product images arrive asynchronously from the LocalImages worker
    // (loaded and scaled on a worker thread, never on the GUI thread).
    if (m_images) {
        connect(m_images, &LocalImages::pixmapReady,
                this, &RegisterWindow::onImageReady);
    }

    // Cart section
    connect(UIMap::discountBtn, &QPushButton::clicked, this, &RegisterWindow::onDiscountClicked);
    connect(UIMap::openTicketsBtn, &QPushButton::clicked, this, &RegisterWindow::onOpenTicketsClicked);
    connect(UIMap::withdrawBtn, &QPushButton::clicked, this, &RegisterWindow::onWithdrawClicked);
    connect(UIMap::confirmBtn, &QPushButton::clicked, this, &RegisterWindow::onConfirmClicked);
    connect(UIMap::cancelBtn, &QPushButton::clicked, this, &RegisterWindow::onCancelClicked);

    // Change notifications
    if (m_notifier) {
        connect(m_notifier, &ChangeNotifier::productChanged,
                this, [this](int) { loadProducts(); m_bell->refreshNow(); });
        connect(m_notifier, &ChangeNotifier::ticketCreated,
                this, [this](int) { updateDailyIncome(); m_bell->refreshNow(); });
        connect(m_notifier, &ChangeNotifier::ticketUpdated,
                this, [this](int) { updateDailyIncome(); m_bell->refreshNow(); });
        connect(m_notifier, &ChangeNotifier::categoryChanged,
                this, [this](int) { loadCategories(); });
        connect(m_notifier, &ChangeNotifier::appNavigateRequested,
                this, &RegisterWindow::handleNavigation);
    }
}

void RegisterWindow::updateUserDisplay()
{
    if (m_session && UIMap::userLabel) {
        QString displayText = QString("%1 (%2)")
        .arg(m_session->displayName())
            .arg(AppHelpers::roleName(m_session->role()));
        UIMap::userLabel->setText(displayText);
    }
}

void RegisterWindow::updateDailyIncome()
{
    if (!m_api || !UIMap::incomeLabel) return;

    double income = m_api->dailyIncome(QDate::currentDate().toString("yyyy-MM-dd"));
    if (!m_api->ok()) {
        qWarning() << "Failed to load daily income:" << m_api->lastError();
        return;
    }

    Animations::animateValues(UIMap::incomeLabel, {income}, [](const QVector<double>& shown) {
        UIMap::incomeLabel->setText(tr("Today: %1").arg(AppHelpers::formatPrice(shown[0])));
    });
}

void RegisterWindow::loadCategories()
{
    if (!m_api || !UIMap::filterBox) return;

    m_categories.clear();
    UIMap::filterBox->clear();

    QList<DataRow> rows = m_api->categories();
    if (!m_api->ok()) {
        qWarning() << "Failed to load categories:" << m_api->lastError();
        return;
    }

    UIMap::filterBox->addItem(tr("All Categories"), -1);

    for (const DataRow& row : rows) {
        QHash<QString, QVariant> category;
        category["id"] = row.value("id");
        category["name"] = row.value("name");
        category["description"] = row.value("description");

        m_categories.append(category);
        UIMap::filterBox->addItem(row.value("name").toString(), row.value("id"));
    }

    qDebug() << "Loaded" << m_categories.size() << "categories";
}

void RegisterWindow::loadProducts()
{
    if (!m_api) return;

    const QList<DataRow> rows = m_api->products();
    if (!m_api->ok()) {
        m_products.clear();
        qWarning() << "Failed to load products:" << m_api->lastError();
        return;
    }

    QList<QHash<QString, QVariant>> loaded;
    loaded.reserve(rows.size());
    for (const DataRow& row : rows) {
        QHash<QString, QVariant> product;
        product["id"] = row.value("id");
        product["name"] = row.value("name");
        product["price"] = row.value("price");
        product["cost"] = row.value("cost");
        product["stock"] = row.value("stock");
        product["category_id"] = row.value("category_id");
        product["category"] = row.value("category_name");
        product["image_path"] = row.value("image_path");
        product["is_service"] = row.value("is_service");
        product["has_variants"] = row.value("has_variants");

        loaded.append(product);
    }

    // Refreshes are frequent (every sale, on every station) and usually change
    // nothing this screen shows: rebuild the grid only when something did. The
    // out-of-stock visibility setting counts, since it can change at any time.
    const bool showOutOfStock = LocalSettings::load().showOosProducts;
    if (m_productsLoaded && loaded == m_products && showOutOfStock == m_loadedShowOutOfStock)
        return;

    m_products = loaded;
    m_productsLoaded = true;
    m_loadedShowOutOfStock = showOutOfStock;
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
        // Deferred images: the button appears instantly with default art;
        // the real image is downloaded progressively after the window is up.
        ProductButton* btn = ProductButtonCreator::createButtonDeferred(product);

        connect(btn, &ProductButton::clicked, this, &RegisterWindow::onProductButtonClicked);

        UIMap::itemsGridLayout->addWidget(btn, row, col, Qt::AlignLeft | Qt::AlignTop);
        m_productButtons.append(btn);

        if (++col >= columns) {
            col = 0;
            row++;
        }
    }

    // Collect the image each button wants (keyed by filename) and serve:
    // cached ones instantly from RAM, the rest asynchronously via the
    // LocalImages worker. The grid itself is already fully built.
    m_buttonsByImage.clear();
    for (QWidget* w : m_productButtons) {
        auto* btn = qobject_cast<ProductButton*>(w);
        if (!btn)
            continue;
        const QString path = btn->property("imagePath").toString();
        if (!path.isEmpty())
            m_buttonsByImage[QFileInfo(path).fileName()].append(btn);
    }

    const int imageSize = RegisterConstants::PRODUCT_IMAGE_SIZE;
    for (auto it = m_buttonsByImage.constBegin(); it != m_buttonsByImage.constEnd(); ++it) {
        const QString name = it.key();
        if (!m_images)
            break;
        if (m_images->isCached(name, imageSize)) {
            const QPixmap pm = m_images->thumbnail(name, imageSize);
            for (const QPointer<ProductButton>& btn : it.value()) {
                if (btn)
                    ProductButtonCreator::applyPixmap(btn, pm);
            }
        } else if (!m_requestedImages.contains(name)) {
            m_requestedImages.insert(name);
            m_images->requestPixmap(name, imageSize);
        }
    }

    // Tiles are fixed width and packed to the left: an empty column after the
    // last one takes the spare width, and an empty row pushes them to the top.
    for (int column = 0; column < UIMap::itemsGridLayout->columnCount(); ++column)
        UIMap::itemsGridLayout->setColumnStretch(column, 0);
    UIMap::itemsGridLayout->setColumnStretch(columns, 1);
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

    // The last column needs no gap after it.
    return qMax(RegisterConstants::MIN_COLUMNS,
                (availableWidth + RegisterConstants::PRODUCT_GRID_SPACING) / cellSize);
}

void RegisterWindow::clearProductGrid()
{
    if (!UIMap::itemsGridLayout) return;  // CHANGED

    // Pending image maps belong to the buttons being destroyed.
    if (m_resizeRebuildTimer) m_resizeRebuildTimer->stop();
    m_buttonsByImage.clear();

    QLayoutItem* item;
    while ((item = UIMap::itemsGridLayout->takeAt(0)) != nullptr) {  // CHANGED
        delete item->widget();
        delete item;
    }
    m_productButtons.clear();
}

void RegisterWindow::onImageReady(const QString& filename, int maxSize, const QImage& image)
{
    if (maxSize != RegisterConstants::PRODUCT_IMAGE_SIZE || image.isNull())
        return;  // not a product-grid request (or missing: default art stays)

    const QPixmap pm = QPixmap::fromImage(image);
    const QList<QPointer<ProductButton>> buttons = m_buttonsByImage.value(filename);
    for (const QPointer<ProductButton>& btn : buttons) {
        if (btn)
            ProductButtonCreator::applyPixmap(btn, pm);
    }
}

QList<QHash<QString, QVariant>> RegisterWindow::filterProducts()
{
    // OOS visibility is a per-machine setting.
    const bool showOOS = LocalSettings::load().showOosProducts;

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
    // Debounce: a drag fires resize events continuously and each rebuild
    // recreates the whole grid; wait until the size settles.
    if (m_resizeRebuildTimer)
        m_resizeRebuildTimer->start();
    updateCheckoutState();
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

        VariantPickerDialog picker(productName, productId, m_api, isService, this);
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
                                tr("Product '%1' is out of stock.").arg(name));
        return;
    }

    // Check if already in cart
    if (m_cart.contains(productId)) {
        if (!isService && m_cart[productId].quantity >= stock) {
            AppHelpers::showWarning(this,
                                    QString(AppHelpers::trMsg(RegisterConstants::MSG_STOCK_EXCEEDED))
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
        AppHelpers::showWarning(this, tr("Variant '%1' is out of stock.").arg(variant.name));
        return;
    }

    if (m_cart.contains(cartKey)) {
        if (!parentIsService && m_cart[cartKey].quantity >= variant.stock) {
            AppHelpers::showWarning(this, QString(AppHelpers::trMsg(RegisterConstants::MSG_STOCK_EXCEEDED))
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

    // Which rows are coming and going, for the animations. A bulk change (a
    // cleared cart, a ticket loaded for editing) just appears at once.
    QSet<int> previousKeys;
    for (int i = 0; i < UIMap::cartLayout->count(); ++i) {
        if (auto* row = qobject_cast<CartItemWidget*>(UIMap::cartLayout->itemAt(i)->widget()))
            previousKeys.insert(row->cartKey());
    }
    QSet<int> addedKeys;
    for (auto it = m_cart.constBegin(); it != m_cart.constEnd(); ++it) {
        if (!previousKeys.contains(it.key()))
            addedKeys.insert(it.key());
    }
    int removedRowIndex = -1;
    QPixmap removedRowPicture;
    if (addedKeys.isEmpty() && !m_cart.isEmpty() && previousKeys.size() == m_cart.size() + 1) {
        for (int i = 0; i < UIMap::cartLayout->count(); ++i) {
            auto* row = qobject_cast<CartItemWidget*>(UIMap::cartLayout->itemAt(i)->widget());
            if (row && !m_cart.contains(row->cartKey())) {
                removedRowIndex = i;
                removedRowPicture = Animations::rowSnapshot(row);
                break;
            }
        }
    }

    // Remove all cart item widgets, keeping the trailing stretch at the end
    while (UIMap::cartLayout->count() > 1) {
        QLayoutItem* item = UIMap::cartLayout->takeAt(0);
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    // Add cart items directly to the layout
    for (const int cartKey : cartKeysInOrder()) {
        const CartItem& item = m_cart[cartKey];

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

        if (addedKeys.size() == 1 && addedKeys.contains(cartKey))
            Animations::rowIn(widget);
    }

    if (removedRowIndex >= 0)
        Animations::collapseRow(UIMap::cartLayout, removedRowIndex, removedRowPicture);

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

    // Update labels
    if (UIMap::totalLabel) {
        Animations::animateValues(UIMap::totalLabel, {subtotal, totalDiscount, m_cartTotal},
                                  [this](const QVector<double>& shown) {
            m_subtotalLabel->setText(AppHelpers::formatAmount(shown[0]));
            m_discountLabel->setText("-" + AppHelpers::formatAmount(shown[1]));
            UIMap::totalLabel->setText(
                QString("%1 %2").arg(AppHelpers::formatAmount(shown[2]), AppConstants::CURRENCY));
        });
    }
    m_totalsDetail->setVisible(totalDiscount > 0);

    updateCheckoutState();
}

// Everything in the cart panel that follows from the cart's contents.
void RegisterWindow::updateCheckoutState()
{
    const bool empty = m_cart.isEmpty();

    int units = 0;
    for (const auto& item : m_cart)
        units += item.quantity;
    m_cartCountLabel->setText(QString("%1 %2").arg(units).arg(units == 1 ? "item" : "items"));
    m_cartCountLabel->setVisible(!empty);

    UIMap::cart->setVisible(!empty);
    m_cartEmptyHint->setVisible(empty);

    // The amount goes on Confirm when it fits. The total above always shows it.
    const QString withAmount = QString("%1  ·  %2 %3").arg(m_confirmText,
                                                           AppHelpers::formatAmount(m_cartTotal),
                                                           AppConstants::CURRENCY);
    const int roomForText = UIMap::confirmBtn->width() - 2 * ConfirmTextMargin;
    const bool amountFits = UIMap::confirmBtn->fontMetrics().horizontalAdvance(withAmount) <= roomForText;
    UIMap::confirmBtn->setText(!empty && amountFits ? withAmount : m_confirmText);
    UIMap::confirmBtn->setEnabled(!empty);
    UIMap::cancelBtn->setEnabled(!empty || !m_discounts.isEmpty() || m_editMode);
}

void RegisterWindow::setCheckoutLabels(const QString& confirm, const QString& cancel)
{
    m_confirmText = confirm;
    UIMap::cancelBtn->setText(cancel);
    updateCheckoutState();
}

QList<int> RegisterWindow::cartKeysInOrder() const
{
    QList<int> keys = m_cart.keys();
    std::sort(keys.begin(), keys.end(), [this](int a, int b) {
        return m_cart[a].addedOrder < m_cart[b].addedOrder;
    });
    return keys;
}

QList<CartItem> RegisterWindow::cartInOrder() const
{
    QList<CartItem> items;
    for (const int key : cartKeysInOrder())
        items << m_cart[key];
    return items;
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
        AppHelpers::showWarning(this, AppHelpers::trMsg(RegisterConstants::MSG_CART_EMPTY));
        return false;
    }

    // Re-check stock against LIVE data before sending: the cart snapshot is
    // taken at add-to-cart time, and another register may have sold the last
    // unit since (the grid refreshes within a second; the cart does not).
    // The sale transaction enforces this too — catching it here keeps the cart intact.
    if (m_api) {
        QStringList shortages;
        for (const auto& item : m_cart) {
            if (item.isService) continue;
            if (item.variantId > 0) {
                const QJsonObject chk = m_api->stockCheckVariant(item.variantId);
                if (!m_api->ok()) continue;   // transport error: the sale call reports it
                if (chk.value("is_service").toBool()) continue;
                const int available = chk.value("stock").toInt();
                if (available < item.quantity)
                    shortages << tr("- %1 (need %2, %3 available)")
                                     .arg(item.name).arg(item.quantity).arg(available);
            } else if (item.originalProductId > 0) {
                const QJsonObject chk = m_api->stockCheckProduct(item.originalProductId);
                if (!m_api->ok()) continue;
                if (chk.value("is_service").toBool()) continue;
                const int available = chk.value("stock_quantity").toInt();
                if (available < item.quantity)
                    shortages << tr("- %1 (need %2, %3 available)")
                                     .arg(item.name).arg(item.quantity).arg(available);
            }
        }
        if (!shortages.isEmpty()) {
            AppHelpers::showWarning(this,
                                    tr("The following items no longer have enough stock:\n\n") +
                                        shortages.join("\n") +
                                        tr("\n\nPlease adjust the cart to continue."));
            loadProducts();   // grid was stale; show what is actually there
            return false;
        }
    }

    return true;
}

void RegisterWindow::processTransaction()
{
    if (!validateTransaction()) return;

    if (!m_session) {
        AppHelpers::showError(this, AppHelpers::trMsg(RegisterConstants::MSG_NO_USER_LOGGED_IN));
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

    // One transaction: ticket + items + stock + flags. It
    // returns the final ticket number (collision-safe across terminals).
    QVariantMap header;
    header["ticket_number"] = AppHelpers::generateTicketNumber();
    header["date"] = QDate::currentDate().toString("yyyy-MM-dd");
    header["time"] = QTime::currentTime().toString("HH:mm:ss");
    header["seller_username"] = m_session->username();
    header["seller_display_name"] = m_session->displayName();
    header["subtotal"] = subtotal;
    header["discount_amount"] = totalDiscount;
    header["tax_amount"] = 0.0;
    header["total"] = total;
    header["payment_method"] = "CASH";
    header["status"] = "COMPLETED";
    header["deposit_amount"] = 0.0;

    QList<QVariantMap> items;
    for (const CartItem& item : cartInOrder()) {
        QVariantMap it;
        it["product_id"] = item.originalProductId > 0 ? QVariant(item.originalProductId) : QVariant();
        it["variant_id"] = item.variantId > 0 ? QVariant(item.variantId) : QVariant();
        it["product_name"] = item.name;
        it["quantity"] = item.quantity;
        it["unit_price"] = item.price;
        it["unit_cost"] = item.cost;
        it["total_price"] = item.price * item.quantity;
        it["is_service"] = item.isService;
        items << it;
    }

    QString ticketNumber;
    int ticketId = m_api->completeSale(header, items, &ticketNumber);
    if (ticketId <= 0) {
        AppHelpers::showError(this,
                              QString("%1\n\n%2")
                                  .arg(AppHelpers::trMsg(RegisterConstants::MSG_TRANSACTION_FAILED))
                                  .arg(m_api->lastError()));
        loadProducts();   // may have been refused on stock: show reality
        return;
    }

    // Notify inventory app of stock changes
    if (m_notifier) {
        m_notifier->notifyChange(ChangeType::PRODUCT_CHANGED, 0);
        m_notifier->notifyChange(ChangeType::TICKET_CREATED, ticketId);
    }

    // This register made the sale itself: refresh right away instead of
    // waiting for the shared-memory loopback (which does not deliver on
    // Windows and fails silently there).
    updateDailyIncome();
    m_bell->refreshNow();

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

    if (AppHelpers::confirmAction(this, AppHelpers::trMsg(RegisterConstants::MSG_CANCEL_CONFIRM))) {
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

void RegisterWindow::onDiscountClicked()
{
    if (m_cart.isEmpty()) {
        AppHelpers::showWarning(this, AppHelpers::trMsg(RegisterConstants::MSG_CART_EMPTY));
        return;
    }

    double subtotal = 0.0;
    for (const auto& item : m_cart)
        subtotal += item.price * item.quantity;

    DiscountDialog dlg(m_api, subtotal, this);
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
    OpenTicketsDialog dlg(m_api, this);
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
    if (m_session && !Permissions::can(m_api, m_session->username(), m_session->role(),
                                       AccessControl::PERMISSION_WITHDRAW_CASH)) {
        AppHelpers::showWarning(this, tr("You do not have permission to withdraw cash."));
        return;
    }

    ModernDialog dlg(ModernDialog::Look::Card, "cash", ModernDialog::Tone::Amber, tr("Withdraw cash"),
                     tr("Take money out of the drawer"), 420, this);
    QLineEdit* amountEdit = ModernDialog::moneyEdit(&dlg, true);
    dlg.addField(tr("Amount"), amountEdit);
    QPushButton* withdrawBtn = dlg.addFooter(tr("Withdraw"), "cash", "amber");
    withdrawBtn->setEnabled(false);
    connect(amountEdit, &QLineEdit::textChanged, withdrawBtn, [amountEdit, withdrawBtn]() {
        withdrawBtn->setEnabled(ModernDialog::moneyValue(amountEdit) > 0);
    });
    connect(withdrawBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    amountEdit->setFocus();

    if (dlg.exec() != QDialog::Accepted) return;

    const double amount = ModernDialog::moneyValue(amountEdit);
    if (amount <= 0) return;

    const QString userName = m_session ? m_session->displayName() : "Unknown";

    if (!m_api->recordWithdrawal(userName, amount, QDate::currentDate().toString("yyyy-MM-dd"))) {
        AppHelpers::showError(this, tr("Failed to record withdrawal.\n\n") + m_api->lastError());
        return;
    }
    // Financial shows withdrawals among the expenses.
    if (m_notifier)
        m_notifier->notifyChange(ChangeType::EXPENSE_UPDATED, 0);

    AppHelpers::showInfo(this, tr("Withdrawal of %1 recorded successfully.").arg(AppHelpers::formatPrice(amount)));
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

void RegisterWindow::onLogoutClicked()
{
    if (!LogoutManager::confirmAndLogout(this, m_session->sessionId())) {
        return;
    }
    close();
    emit logoutRequested();
}

// ===== EDIT MODE =====

void RegisterWindow::handleNavigation(const QString& action)
{
    NotificationActions::Action parsed = NotificationActions::parse(action);

    // Bring window to front reliably (also handles "Register|show" focus requests)
    AppHelpers::bringToFront(this);

    if (parsed.app.compare("Register", Qt::CaseInsensitive) != 0 || parsed.value <= 0)
        return;

    const int ticketId = parsed.value;

    // Warn if cart is not empty
    if (!m_cart.isEmpty()) {
        if (!AppHelpers::confirmAction(this,
                                       tr("There are items in the current cart.\n"
                                       "Loading the ticket to edit will clear the cart. Continue?")))
            return;
    }

    // Route to open-ticket edit mode for OPEN tickets, plain edit otherwise.
    DataRow meta = m_api->ticket(ticketId);
    if (!meta.isEmpty() &&
        QString::compare(meta.value("status").toString(), "OPEN", Qt::CaseInsensitive) == 0) {
        enterOpenTicketEditMode(ticketId);
    } else {
        enterEditMode(ticketId);
    }
}

void RegisterWindow::enterEditMode(int ticketId)
{
    if (!m_api) return;

    clearCart();

    // Load ticket items into the cart
    QList<DataRow> rows = m_api->ticketItems(ticketId);
    if (!m_api->ok()) {
        qWarning() << "enterEditMode: failed to load ticket items:" << m_api->lastError();
        AppHelpers::showError(this, tr("Failed to load ticket items for editing."));
        return;
    }

    for (const DataRow& row : rows) {
        int     productId  = row.value("product_id").toInt();
        int     variantId  = row.value("variant_id").toInt();
        QString name       = row.value("product_name").toString();
        QString varName    = row.value("variant_name").toString();
        int     qty        = row.value("quantity").toInt();
        double  unitPrice  = row.value("unit_price").toDouble();
        double  unitCost   = row.value("unit_cost").toDouble();

        CartItem item;
        item.name              = varName.isEmpty() ? name : QString("%1 — %2").arg(name, varName);
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
    setCheckoutLabels(tr("Save Changes"), tr("Cancel Edit"));
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

    setCheckoutLabels(tr("Confirm"), tr("Cancel"));
    setWindowTitle(tr("POSBox - Register"));

    qDebug() << "Exited edit mode";
}

void RegisterWindow::saveEditedTransaction()
{
    if (!validateTransaction()) return;
    if (!m_api || m_editTicketId <= 0) return;

    // ── Recalculate totals ────────────────────────────────────────────────────
    double subtotal = 0.0;
    for (const auto& item : m_cart)
        subtotal += item.price * item.quantity;

    double totalDiscount = 0.0;
    for (const DiscountItem& discount : m_discounts)
        totalDiscount += DiscountHandler::calculateAmount(discount, subtotal);

    double total = qMax(0.0, subtotal - totalDiscount);

    // ── One transaction: totals + item replace + stock diffs ───────────────
    QVariantMap totals;
    totals["subtotal"] = subtotal;
    totals["discount_amount"] = totalDiscount;
    totals["total"] = total;

    QList<QVariantMap> items;
    for (const CartItem& item : cartInOrder()) {
        QVariantMap it;
        it["product_id"] = item.originalProductId > 0 ? QVariant(item.originalProductId) : QVariant();
        it["variant_id"] = item.variantId > 0 ? QVariant(item.variantId) : QVariant();
        it["product_name"] = item.name;
        it["quantity"] = item.quantity;
        it["unit_price"] = item.price;
        it["unit_cost"] = item.cost;
        it["total_price"] = item.price * item.quantity;
        it["is_service"] = item.isService;
        items << it;
    }

    if (!m_api->editTicket(m_editTicketId, totals, items)) {
        AppHelpers::showError(this, tr("Failed to update ticket.\n\n") + m_api->lastError());
        return;
    }

    // ── Notify other apps on this machine to refresh ──────────────────────────
    if (m_notifier) {
        m_notifier->notifyChange(ChangeType::PRODUCT_CHANGED, 0);
        m_notifier->notifyChange(ChangeType::TICKET_UPDATED, m_editTicketId);
    }

    // Direct refresh: the edit can change today's totals (no loopback wait).
    updateDailyIncome();
    m_bell->refreshNow();

    AppHelpers::showInfo(this, tr("Ticket updated successfully."));

    exitEditMode();
    loadProducts();
}

// Stock diffs are applied atomically inside ticket_edit.
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
    if (!m_session || !m_api) return;

    // If we're editing an existing open ticket, pre-fill the dialog
    SaveTicketDialog dlg(this);
    {
        // What the ticket holds, for the side panel.
        double subtotal = 0.0;
        for (const auto& item : m_cart)
            subtotal += item.price * item.quantity;
        double discount = 0.0;
        for (const DiscountItem& d : m_discounts)
            discount += DiscountHandler::calculateAmount(d, subtotal);
        QList<SaveTicketDialog::Line> lines;
        for (const auto& item : cartInOrder())
            lines.append({QString("%1 × %2").arg(item.name).arg(item.quantity), item.price * item.quantity});
        dlg.setSummary(lines, discount, qMax(0.0, subtotal - discount));
    }
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

    QList<QVariantMap> items;
    for (const CartItem& item : cartInOrder()) {
        QVariantMap it;
        it["product_id"] = item.originalProductId > 0 ? QVariant(item.originalProductId) : QVariant();
        it["variant_id"] = item.variantId > 0 ? QVariant(item.variantId) : QVariant();
        it["product_name"] = item.name;
        it["quantity"] = item.quantity;
        it["unit_price"] = item.price;
        it["unit_cost"] = item.cost;
        it["total_price"] = item.price * item.quantity;
        it["is_service"] = item.isService;
        items << it;
    }

    if (m_openTicketEditMode) {
        // ── Overwrite existing open ticket ───────────────────────────────────
        QVariantMap header;
        header["subtotal"] = subtotal;
        header["discount_amount"] = totalDiscount;
        header["total"] = total;
        header["customer_name"] = customerName.isEmpty() ? QVariant() : QVariant(customerName);
        header["deposit_amount"] = advance;
        header["description"] = description.isEmpty() ? QVariant() : QVariant(description);

        if (!m_api->updateOpenTicket(m_openTicketEditId, header, items)) {
            AppHelpers::showError(this, tr("Failed to update ticket.\n\n") + m_api->lastError());
            return;
        }

        if (m_notifier)
            m_notifier->notifyChange(ChangeType::TICKET_UPDATED, m_openTicketEditId);

        // Direct refresh (no shared-memory loopback wait).
        updateDailyIncome();
        m_bell->refreshNow();

        AppHelpers::showInfo(this, tr("Open ticket updated successfully."));

        // Exit open ticket edit mode
        m_openTicketEditMode = false;
        m_openTicketEditId   = -1;
        m_openTicketCustomerName.clear();
        m_openTicketAdvance  = 0.0;
        m_openTicketDescription.clear();

    } else {
        // ── Create new open ticket ────────────────────────────────────────────
        QVariantMap header;
        header["ticket_number"] = AppHelpers::generateTicketNumber();
        header["date"] = QDate::currentDate().toString("yyyy-MM-dd");
        header["time"] = QTime::currentTime().toString("HH:mm:ss");
        header["seller_username"] = m_session->username();
        header["seller_display_name"] = m_session->displayName();
        header["subtotal"] = subtotal;
        header["discount_amount"] = totalDiscount;
        header["tax_amount"] = 0.0;
        header["total"] = total;
        header["payment_method"] = "CASH";
        header["status"] = "OPEN";
        header["customer_name"] = customerName.isEmpty() ? QVariant() : QVariant(customerName);
        header["deposit_amount"] = advance;
        header["description"] = description.isEmpty() ? QVariant() : QVariant(description);

        QString ticketNumber;
        const int ticketId = m_api->saveOpenTicket(header, items, &ticketNumber);

        if (ticketId <= 0) {
            AppHelpers::showError(this, tr("Failed to save ticket.\n\n") + m_api->lastError());
            return;
        }

        if (m_notifier)
            m_notifier->notifyChange(ChangeType::TICKET_CREATED, ticketId);

        // Direct refresh (no shared-memory loopback wait).
        updateDailyIncome();
        m_bell->refreshNow();

        AppHelpers::showInfo(this, tr("Ticket #%1 saved as open.").arg(ticketNumber));
    }

    clearCart();
    loadProducts();
}

void RegisterWindow::completeOpenTicket(int ticketId)
{
    if (!m_api || ticketId <= 0) return;

    // ── Check stock before doing anything ────────────────────────────────────
    QList<DataRow> itemRows = m_api->ticketItems(ticketId);
    if (!m_api->ok()) {
        AppHelpers::showError(this, tr("Failed to load ticket items."));
        return;
    }

    QStringList insufficientItems;

    for (const DataRow& row : itemRows) {
        const int    productId = row.value("product_id").toInt();
        const int    variantId = row.value("variant_id").toInt();
        const int    qty       = row.value("quantity").toInt();
        const QString name     = row.value("product_name").toString();

        if (variantId > 0) {
            // Join with parent product to check is_service before checking stock
            QJsonObject chk = m_api->stockCheckVariant(variantId);
            if (!m_api->ok()) continue;
            if (!chk.value("is_service").toBool() &&
                chk.value("stock").toInt() < qty)
                insufficientItems << tr("- %1 (need %2)").arg(name).arg(qty);
        } else if (productId > 0) {
            QJsonObject chk = m_api->stockCheckProduct(productId);
            if (!m_api->ok()) continue;
            if (!chk.value("is_service").toBool() &&
                chk.value("stock_quantity").toInt() < qty)
                insufficientItems << tr("- %1 (need %2)").arg(name).arg(qty);
        }
    }

    if (!insufficientItems.isEmpty()) {
        AppHelpers::showWarning(this,
                                tr("The following items no longer have enough stock:\n\n") +
                                    insufficientItems.join("\n") +
                                    tr("\n\nPlease edit the ticket before completing."));
        return;
    }

    // ── All stock OK — mark as completed and deduct (one atomic call) ────────
    if (!m_api->completeOpenTicket(ticketId)) {
        AppHelpers::showError(this, tr("Failed to complete ticket.\n\n") + m_api->lastError());
        return;
    }

    if (m_notifier) {
        m_notifier->notifyChange(ChangeType::PRODUCT_CHANGED, 0);
        m_notifier->notifyChange(ChangeType::TICKET_UPDATED, ticketId);
    }

    // Direct refresh: completing the open ticket adds today's income
    // (no shared-memory loopback wait).
    updateDailyIncome();
    m_bell->refreshNow();

    showTicketCompletedDialog(ticketId);
    loadProducts();
}

// Stock deduction for completed open tickets runs atomically inside the
// ticket_open_complete transaction — no separate deduct pass exists.
void RegisterWindow::enterOpenTicketEditMode(int ticketId)
{
    if (!m_api || ticketId <= 0) return;

    if (!m_cart.isEmpty()) {
        if (!AppHelpers::confirmAction(this,
                                       tr("There are items in the current cart.\n"
                                       "Loading the ticket to edit will clear the cart. Continue?")))
            return;
    }

    // Load ticket metadata
    DataRow meta = m_api->ticket(ticketId);
    if (meta.isEmpty()) {
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
    setCheckoutLabels(tr("Confirm"), tr("Cancel"));
    setWindowTitle(tr("POSBox - Register  [EDITING OPEN TICKET]"));
    updateOpenTicketButton();
}

// ===== TICKET COMPLETED DIALOG =====

void RegisterWindow::showTicketCompletedDialog(int ticketId)
{
    const DataRow ticket = m_api->ticket(ticketId);
    const QString number = ticket.value("ticket_number").toString();
    MessageDialog dialog(MessageDialog::Kind::Success, tr("Sale completed"),
                         number.isEmpty() ? tr("Stock updated") : tr("Ticket #%1").arg(number), this);
    if (!ticket.isEmpty())
        dialog.setAmount(AppHelpers::formatPrice(ticket.value("total").toDouble()));
    dialog.setButtons(tr("Print Ticket"), tr("Close"), "printer");

    if (dialog.exec() == QDialog::Accepted)
        printTicket(ticketId);
}

void RegisterWindow::printTicket(int ticketId)
{
    if (!m_api || ticketId <= 0) return;

    const ThermalPrinter::TicketData data = Receipt::loadTicket(m_api, m_images, ticketId);

    ThermalPrinter printer(LocalSettings::load().printer);
    if (!printer.print(data)) {
        AppHelpers::showError(this, tr("Failed to print ticket.\n\n") + printer.lastError());
    }
}

// Out-of-stock flags are maintained atomically inside every
// stock movement — no client-side tracking remains.
