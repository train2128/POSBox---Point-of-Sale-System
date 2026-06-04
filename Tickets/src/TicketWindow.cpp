#include "TicketWindow.h"
#include "ui_TicketWindow.h"
#include "UserSession.h"
#include "SessionManager.h"
#include "StyleManager.h"
#include "DatabaseManager.h"
#include "QueryRepository.h"
#include "ChangeNotifier.h"
#include "TicketEditRequest.h"
#include "RegisterLock.h"
#include "TicketPrintManager.h"
#include "ThermalPrinter.h"
#include "CustomerInvoiceDialog.h"
#include "CommonConfig.h"
#include "PrinterConstants.h"
#include "Prefixes.h"
#include "AccessControl.h"
#include "LogoutManager.h"
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QHeaderView>
#include <QSqlQuery>
#include <QSqlError>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFrame>
#include <QProcess>
#include <QCoreApplication>
#include <QApplication>
#include <QDialog>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <QIcon>

// ── Column indices ─────────────────────────────────────────────────────────
static const int COL_TICKET_NUM = 0;
static const int COL_DATE       = 1;
static const int COL_TIME       = 2;
static const int COL_SELLER     = 3;
static const int COL_TOTAL      = 4;
static const int COL_DISCOUNT   = 5;
static const int COL_STATUS     = 6;
static const int NUM_COLUMNS    = 7;

TicketWindow::TicketWindow(UserSession* session,
                           DatabaseManager* dbManager,
                           ChangeNotifier* notifier,
                           QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::TicketWindow)
    , m_session(session)
    , m_db(dbManager)
    , m_notifier(notifier)
    , m_model(nullptr)
    , m_proxy(nullptr)
    , m_selectedTicketId(-1)
    , m_printManager(nullptr)
{
    ui->setupUi(this);
    StyleManager::applyGlobalStyles(this, m_db->database());
    m_printManager = new TicketPrintManager(m_db, this);
    setupUI();
    setupTable();
    loadTickets(QDate::currentDate());
    showMaximized();
}

TicketWindow::~TicketWindow()
{
    delete ui;
}

void TicketWindow::setupUI()
{
    setWindowTitle("Ticket History");

    // Logout button (icon set via StyleManager)
    ui->logoutBtn->setText("");
    ui->logoutBtn->setIcon(QIcon(StyleManager::iconPath("logout")));
    ui->logoutBtn->setIconSize(QSize(22, 22));
    ui->logoutBtn->setFixedSize(36, 36);
    ui->logoutBtn->setToolTip("Logout");

    // Action buttons
    ui->printTicketBtn->setText("Print");
    ui->editTicketBtn->setText("Edit");
    ui->refundTicketBtn->setText("Refund");
    ui->deleteTicketBtn->setText("Delete");

    ui->printTicketBtn->setEnabled(false);
    ui->editTicketBtn->setEnabled(false);
    ui->refundTicketBtn->setEnabled(false);
    ui->deleteTicketBtn->setEnabled(false);

    // Search bar
    ui->searchBar->setPlaceholderText("Search by ticket #, seller, date, or status...");

    // Date filter — default to today
    ui->dateSelect->setDate(QDate::currentDate());
    ui->dateSelect->setCalendarPopup(true);
    ui->dateSelect->setDisplayFormat("yyyy-MM-dd");

    updateUserDisplay();

    // Hide delete button if user lacks permission
    if (m_session) {
        bool canDelete = AccessControl::canPerformAction(
            m_session->username(), AccessControl::DeleteTicket, m_db->database());
        ui->deleteTicketBtn->setVisible(canDelete);
    } else {
        ui->deleteTicketBtn->setVisible(false);
    }

    connect(ui->logoutBtn,       &QPushButton::clicked, this, &TicketWindow::onLogoutClicked);
    connect(ui->dateSelect,      &QDateEdit::dateChanged, this, &TicketWindow::onDateChanged);
    connect(ui->searchBar,       &QLineEdit::textChanged, this, &TicketWindow::onSearchChanged);
    connect(ui->deleteTicketBtn, &QPushButton::clicked, this, &TicketWindow::onDeleteClicked);
    connect(ui->refundTicketBtn, &QPushButton::clicked, this, &TicketWindow::onRefundClicked);
    connect(ui->editTicketBtn,   &QPushButton::clicked, this, &TicketWindow::onEditClicked);
    connect(ui->printTicketBtn,  &QPushButton::clicked, this, &TicketWindow::onPrintClicked);

    if (m_notifier) {
        connect(m_notifier, &ChangeNotifier::ticketUpdated, this, [this](int) {
            showNormal();
            raise();
            activateWindow();
            QWidget::activateWindow();
        });
    }

    // Trailing stretch keeps cart items packed at the top
    ui->ticketCartItems->addStretch(1);
}

void TicketWindow::setupTable()
{
    m_model = new QStandardItemModel(0, NUM_COLUMNS, this);
    m_model->setHorizontalHeaderLabels({
        "Ticket #", "Date", "Time", "Seller", "Total", "Discount", "Status"
    });

    m_proxy = new QSortFilterProxyModel(this);
    m_proxy->setSourceModel(m_model);
    m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxy->setFilterKeyColumn(-1); // search all columns

    ui->ticketList->setModel(m_proxy);
    ui->ticketList->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->ticketList->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->ticketList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->ticketList->setAlternatingRowColors(true);
    ui->ticketList->setSortingEnabled(true);
    ui->ticketList->verticalHeader()->setVisible(false);
    ui->ticketList->horizontalHeader()->setStretchLastSection(false);
    ui->ticketList->horizontalHeader()->setSectionResizeMode(COL_TICKET_NUM, QHeaderView::ResizeToContents);
    ui->ticketList->horizontalHeader()->setSectionResizeMode(COL_DATE,       QHeaderView::ResizeToContents);
    ui->ticketList->horizontalHeader()->setSectionResizeMode(COL_TIME,       QHeaderView::ResizeToContents);
    ui->ticketList->horizontalHeader()->setSectionResizeMode(COL_SELLER,     QHeaderView::Stretch);
    ui->ticketList->horizontalHeader()->setSectionResizeMode(COL_TOTAL,      QHeaderView::ResizeToContents);
    ui->ticketList->horizontalHeader()->setSectionResizeMode(COL_DISCOUNT,   QHeaderView::ResizeToContents);
    ui->ticketList->horizontalHeader()->setSectionResizeMode(COL_STATUS,     QHeaderView::ResizeToContents);

    connect(ui->ticketList->selectionModel(), &QItemSelectionModel::currentRowChanged,
            this, &TicketWindow::onTicketSelected);
}

void TicketWindow::loadTickets(const QDate& date)
{
    if (!m_db || !m_model) return;

    m_model->removeRows(0, m_model->rowCount());

    QSqlQuery query(m_db->database());
    query.prepare(Queries::TICKET_SELECT_BY_DATE);
    query.addBindValue(date.toString("yyyy-MM-dd"));

    if (!query.exec()) {
        qWarning() << "TicketWindow: failed to load tickets:" << query.lastError().text();
        return;
    }

    while (query.next()) {
        int     id         = query.value("id").toInt();
        QString ticketNum  = query.value("ticket_number").toString();
        QString dateStr    = query.value("date").toString();
        QString timeStr    = query.value("time").toString();
        QString seller     = query.value("seller_display_name").toString();
        double  total      = query.value("total").toDouble();
        double  discount   = query.value("discount_amount").toDouble();
        QString status     = query.value("status").toString();

        QList<QStandardItem*> row;

        auto item = [](const QString& text) {
            QStandardItem* i = new QStandardItem(text);
            i->setTextAlignment(Qt::AlignCenter);
            return i;
        };

        QStandardItem* numItem = item(ticketNum);
        numItem->setData(id, Qt::UserRole); // store DB id for later use

        row << numItem
            << item(dateStr)
            << item(timeStr)
            << item(seller)
            << item(AppHelpers::formatPrice(total))
            << item(discount > 0 ? AppHelpers::formatPrice(discount) : "-")
            << item(status);

        m_model->appendRow(row);
    }

    qDebug() << "TicketWindow: loaded" << m_model->rowCount() << "tickets for" << date.toString("yyyy-MM-dd");
}

void TicketWindow::updateUserDisplay()
{
    if (m_session) {
        ui->userLabel->setText(
            QString("%1 (%2)").arg(m_session->displayName()).arg(m_session->role()));
    }
}

void TicketWindow::onDateChanged(const QDate& date)
{
    loadTickets(date);
}

void TicketWindow::onSearchChanged(const QString& text)
{
    m_proxy->setFilterFixedString(text);
}

void TicketWindow::onLogoutClicked()
{
    if (!LogoutManager::confirmAndLogout(this, m_session->sessionId(), m_session->username())) {
        return;
    }
    close();
    emit logoutRequested();
}

void TicketWindow::onTicketSelected(const QModelIndex& current, const QModelIndex& /*previous*/)
{
    if (!current.isValid()) {
        clearTicketCart();
        setActionButtonsEnabled(false);
        m_selectedTicketId = -1;
        return;
    }

    // Map proxy index back to source to get the stored ticket id
    QModelIndex sourceIndex = m_proxy->mapToSource(current);
    QStandardItem* numItem  = m_model->item(sourceIndex.row(), COL_TICKET_NUM);
    if (!numItem) return;

    m_selectedTicketId = numItem->data(Qt::UserRole).toInt();
    loadTicketItems(m_selectedTicketId);
    setActionButtonsEnabled(true);
}

void TicketWindow::loadTicketItems(int ticketId)
{
    clearTicketCart();
    if (!m_db || ticketId <= 0) return;

    QSqlQuery query(m_db->database());
    query.prepare(Queries::TICKET_ITEM_SELECT_BY_TICKET);
    query.addBindValue(ticketId);

    if (!query.exec()) {
        qWarning() << "TicketWindow: failed to load ticket items:" << query.lastError().text();
        return;
    }

    while (query.next()) {
        QString name     = query.value("product_name").toString();
        QString varName  = query.value("variant_name").toString();
        int     qty      = query.value("quantity").toInt();
        double  total    = query.value("total_price").toDouble();

        // Build display name — append variant if present
        QString displayName = varName.isEmpty() ? name : QString("%1 — %2").arg(name, varName);

        // Row widget
        QWidget* row = new QWidget();
        row->setObjectName("ticket-cart-item");

        QHBoxLayout* layout = new QHBoxLayout(row);
        layout->setContentsMargins(10, 6, 10, 6);
        layout->setSpacing(8);

        QLabel* nameLabel = new QLabel(displayName, row);
        nameLabel->setObjectName("ticket-cart-item-name");
        nameLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

        QLabel* qtyLabel = new QLabel(QString("x%1").arg(qty), row);
        qtyLabel->setObjectName("ticket-cart-item-qty");
        qtyLabel->setAlignment(Qt::AlignCenter);

        QLabel* priceLabel = new QLabel(AppHelpers::formatPrice(total), row);
        priceLabel->setObjectName("ticket-cart-item-price");
        priceLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        layout->addWidget(nameLabel, 1);
        layout->addWidget(qtyLabel, 0);
        layout->addWidget(priceLabel, 0);

        // Insert before the trailing stretch
        ui->ticketCartItems->insertWidget(ui->ticketCartItems->count() - 1, row);
    }
}

void TicketWindow::clearTicketCart()
{
    // Remove all widgets except the trailing stretch (last item)
    while (ui->ticketCartItems->count() > 1) {
        QLayoutItem* item = ui->ticketCartItems->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
}

void TicketWindow::setActionButtonsEnabled(bool enabled)
{
    ui->printTicketBtn->setEnabled(enabled);
    ui->editTicketBtn->setEnabled(enabled);
    ui->refundTicketBtn->setEnabled(enabled);
    // deleteTicketBtn — only enable if visible (visibility already checked in setupUI)
    if (ui->deleteTicketBtn->isVisible())
        ui->deleteTicketBtn->setEnabled(enabled);
}

void TicketWindow::onDeleteClicked()
{
    if (m_selectedTicketId <= 0) return;

    if (m_session && !AccessControl::canPerformAction(
            m_session->username(), AccessControl::DeleteTicket, m_db->database())) {
        AppHelpers::showWarning(this, "You do not have permission to delete tickets.");
        return;
    }

    if (!AppHelpers::confirmAction(this, "Are you sure you want to permanently delete this ticket?\nThis action cannot be undone."))
        return;

    QSqlQuery q(m_db->database());

    // Restore stock before deleting (same as refund)
    restoreStock(m_selectedTicketId);

    // Delete ticket items first
    q.prepare(Queries::TICKET_ITEM_DELETE_BY_TICKET);
    q.addBindValue(m_selectedTicketId);
    if (!q.exec()) {
        qWarning() << "Failed to delete ticket items:" << q.lastError().text();
        AppHelpers::showError(this, "Failed to delete ticket items.");
        return;
    }

    // Delete ticket
    q.prepare(Queries::TICKET_DELETE);
    q.addBindValue(m_selectedTicketId);
    if (!q.exec()) {
        qWarning() << "Failed to delete ticket:" << q.lastError().text();
        AppHelpers::showError(this, "Failed to delete ticket.");
        return;
    }

    m_selectedTicketId = -1;
    clearTicketCart();
    setActionButtonsEnabled(false);
    loadTickets(ui->dateSelect->date());

    // Notify Register to refresh in case stock-affecting items were involved
    if (m_notifier)
        m_notifier->notifyChange(ChangeType::PRODUCT_CHANGED, 0);
}

void TicketWindow::onRefundClicked()
{
    if (m_selectedTicketId <= 0) return;

    if (!AppHelpers::confirmAction(this, "Mark this ticket as refunded and restore stock?"))
        return;

    // Update status to REFUNDED
    QSqlQuery q(m_db->database());
    q.prepare(Queries::TICKET_UPDATE_STATUS);
    q.addBindValue("REFUNDED");
    q.addBindValue(m_selectedTicketId);

    if (!q.exec()) {
        qWarning() << "Failed to update ticket status:" << q.lastError().text();
        AppHelpers::showError(this, "Failed to mark ticket as refunded.");
        return;
    }

    // Restore stock for all sold items
    restoreStock(m_selectedTicketId);

    // Notify Register to refresh product stock display
    if (m_notifier)
        m_notifier->notifyChange(ChangeType::PRODUCT_CHANGED, 0);

    // Refresh the table and keep the same date
    loadTickets(ui->dateSelect->date());

    // Clear cart since status changed
    clearTicketCart();
    setActionButtonsEnabled(false);
    m_selectedTicketId = -1;
}

void TicketWindow::restoreStock(int ticketId)
{
    QSqlQuery query(m_db->database());
    query.prepare(Queries::TICKET_ITEM_SELECT_BY_TICKET);
    query.addBindValue(ticketId);

    if (!query.exec()) {
        qWarning() << "restoreStock: failed to fetch items:" << query.lastError().text();
        return;
    }

    while (query.next()) {
        int productId = query.value("product_id").toInt();
        int variantId = query.value("variant_id").toInt();
        int qty       = query.value("quantity").toInt();

        if (variantId > 0) {
            // Restore variant stock
            QSqlQuery sq(m_db->database());
            sq.prepare(Queries::VARIANT_UPDATE_STOCK);
            sq.addBindValue(qty);   // positive = restore
            sq.addBindValue(variantId);
            if (!sq.exec())
                qWarning() << "restoreStock: failed to restore variant" << variantId << sq.lastError().text();

        } else if (productId > 0) {
            // Restore product stock
            QSqlQuery sq(m_db->database());
            sq.prepare(Queries::PRODUCT_UPDATE_STOCK);
            sq.addBindValue(qty);   // positive = restore
            sq.addBindValue(productId);
            if (!sq.exec())
                qWarning() << "restoreStock: failed to restore product" << productId << sq.lastError().text();
        }
        // productId == 0 and variantId == 0 → custom item, no stock to restore
    }
}

void TicketWindow::onEditClicked()
{
    if (m_selectedTicketId <= 0) return;
    if (!m_session) return;

    // Write the edit request JSON file
    if (!TicketEditRequest::write(m_selectedTicketId, m_session->username())) {
        AppHelpers::showError(this, "Failed to create edit request file.");
        return;
    }

    if (RegisterLock::isRunning()) {
        // Register is already open — notify it via shared memory, it will pick up the JSON file
        if (m_notifier)
            m_notifier->notifyChange(ChangeType::TICKET_EDIT_REQUESTED, m_selectedTicketId);
    } else {
        // Register is not running — launch it, it will check the JSON file on startup
        QString executable = QCoreApplication::applicationDirPath() + "/../Register/Register";

        QStringList args;
        args << "--user" << m_session->username();

        if (!QProcess::startDetached(executable, args)) {
            AppHelpers::showError(this, "Failed to launch Register application.");
            TicketEditRequest::clear();
        }
    }
}

void TicketWindow::onPrintClicked()
{
    if (m_selectedTicketId <= 0) return;

    // ── Check whether thermal printer is enabled ──────────────────────────────
    // Read from database. If the query fails or enabled is 0,
    // the printer is considered disabled and we go straight to the invoice preview.
    bool printerEnabled = false;
    {
        QSqlQuery query(m_db->database());
        query.exec("SELECT enabled FROM printer_settings WHERE id = 1");
        if (query.next()) {
            printerEnabled = query.value(0).toInt() != 0;
        }
    }

    // ── Printer disabled → straight to invoice, no choice dialog ─────────────
    if (!printerEnabled) {
        CustomerInvoiceDialog custDlg(this);
        if (custDlg.exec() != QDialog::Accepted) return;

        CustomerInvoiceData customerData = custDlg.getCustomerData();
        m_printManager->printTicketWithPreview(m_selectedTicketId, customerData);
        return;
    }

    // ── Printer enabled → show choice dialog ─────────────────────────────────
    QDialog choiceDialog(this);
    choiceDialog.setWindowTitle("Print");
    choiceDialog.setFixedSize(280, 120);

    QVBoxLayout* layout = new QVBoxLayout(&choiceDialog);
    layout->setSpacing(10);
    layout->setContentsMargins(20, 20, 20, 20);

    QLabel* label = new QLabel("Choose print format:", &choiceDialog);
    label->setAlignment(Qt::AlignCenter);
    layout->addWidget(label);

    QHBoxLayout* btnLayout = new QHBoxLayout();
    QPushButton* invoiceBtn = new QPushButton("Invoice", &choiceDialog);
    QPushButton* ticketBtn  = new QPushButton("Ticket",  &choiceDialog);
    btnLayout->addWidget(invoiceBtn);
    btnLayout->addWidget(ticketBtn);
    layout->addLayout(btnLayout);

    bool invoiceChosen = false;
    bool ticketChosen  = false;

    connect(invoiceBtn, &QPushButton::clicked, [&]() { invoiceChosen = true; choiceDialog.accept(); });
    connect(ticketBtn,  &QPushButton::clicked, [&]() { ticketChosen  = true; choiceDialog.accept(); });

    choiceDialog.exec();

    // ── Invoice ───────────────────────────────────────────────────────────────
    if (invoiceChosen) {
        CustomerInvoiceDialog custDlg(this);
        if (custDlg.exec() != QDialog::Accepted) return;

        CustomerInvoiceData customerData = custDlg.getCustomerData();
        m_printManager->printTicketWithPreview(m_selectedTicketId, customerData);
        return;
    }

    // ── Thermal ticket ────────────────────────────────────────────────────────
    if (!ticketChosen) return;

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
    {
        QSqlQuery q(m_db->database());
        q.prepare(Queries::TICKET_SELECT_BY_ID);
        q.addBindValue(m_selectedTicketId);
        if (q.exec() && q.next()) {
            subtotal       = q.value("subtotal").toDouble();
            discountAmount = q.value("discount_amount").toDouble();
            total          = q.value("total").toDouble();
        }
    }

    // Fetch ticket items
    QList<ThermalPrinter::TicketItem> items;
    {
        QSqlQuery q(m_db->database());
        q.prepare(Queries::TICKET_ITEM_SELECT_BY_TICKET);
        q.addBindValue(m_selectedTicketId);
        if (q.exec()) {
            while (q.next()) {
                ThermalPrinter::TicketItem item;
                QString name    = q.value("product_name").toString();
                QString variant = q.value("variant_name").toString();
                item.name       = variant.isEmpty() ? name : QString("%1 - %2").arg(name, variant);
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
    data.isUnpaid       = false;

    ThermalPrinter printer(m_db->database());
    if (!printer.print(data)) {
        AppHelpers::showError(this, printer.lastError());
    }
}
