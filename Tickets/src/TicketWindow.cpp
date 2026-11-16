#include "TicketWindow.h"
#include "ui_TicketWindow.h"
#include "UserSession.h"
#include "SessionManager.h"
#include "StyleManager.h"
#include "LocalApi.h"
#include "LocalImages.h"
#include "LocalSettings.h"
#include "ChangeNotifier.h"
#include "TicketPrintManager.h"
#include "NotificationManager.h"
#include "NotificationBell.h"
#include "ThermalPrinter.h"
#include "LineIcons.h"
#include "ModernDialog.h"
#include "CommonConfig.h"
#include "PrinterConstants.h"
#include "Prefixes.h"
#include "AccessControl.h"
#include "Permissions.h"
#include "LogoutManager.h"
#include "Animations.h"
#include "LineIcons.h"
#include "TableDelegates.h"
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QHeaderView>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFrame>
#include <QApplication>
#include <QDialog>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <QIcon>
#include <QTimer>
#include <QButtonGroup>
#include <QPainter>
#include <QPainterPath>

// ── Column indices ─────────────────────────────────────────────────────────
static const int COL_TICKET_NUM = 0;
static const int COL_DATE       = 1;
static const int COL_TIME       = 2;
static const int COL_SELLER     = 3;
static const int COL_TOTAL      = 4;
static const int COL_DISCOUNT   = 5;
static const int COL_STATUS     = 6;
static const int NUM_COLUMNS    = 7;

// Extra values kept on the ticket number item for the receipt panel
static const int ROLE_SUBTOTAL = Qt::UserRole + 1;

namespace {

// Status buttons above the list narrow what the search box finds.
class TicketFilterProxy : public QSortFilterProxyModel
{
public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

    void setStatus(const QString& status)
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
        beginFilterChange();
        m_status = status;
        endFilterChange(QSortFilterProxyModel::Direction::Rows);
#else
        m_status = status;
        invalidateFilter();
#endif
    }

protected:
    bool filterAcceptsRow(int row, const QModelIndex& parent) const override
    {
        if (!m_status.isEmpty()
            && sourceModel()->index(row, COL_STATUS, parent).data().toString() != m_status)
            return false;
        return QSortFilterProxyModel::filterAcceptsRow(row, parent);
    }

private:
    QString m_status;
};

// A printed-receipt look: paper with a torn bottom edge.
class ReceiptPaper : public QFrame
{
public:
    explicit ReceiptPaper(QWidget* parent = nullptr)
        : QFrame(parent)
    {
        setObjectName("receiptPaper");
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        constexpr qreal Tooth = 8.0;
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const qreal w = width();
        const qreal h = height();
        QPainterPath path;
        path.moveTo(0, Tooth);
        path.quadTo(0, 0, Tooth, 0);
        path.lineTo(w - Tooth, 0);
        path.quadTo(w, 0, w, Tooth);
        path.lineTo(w, h - Tooth);
        const int teeth = qMax(1, int(w / (Tooth * 2)));
        const qreal step = w / teeth;
        for (int i = 0; i < teeth; ++i) {
            path.lineTo(w - step * (i + 0.5), h);
            path.lineTo(w - step * (i + 1), h - Tooth);
        }
        path.closeSubpath();
        painter.fillPath(path, QColor("#FFFDF8"));
        painter.setPen(QPen(QColor(0, 0, 0, 45), 1));
        painter.drawPath(path);
    }
};

class DashedLine : public QWidget
{
public:
    explicit DashedLine(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setFixedHeight(9);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setPen(QPen(QColor("#9A9A9A"), 1, Qt::DashLine));
        painter.drawLine(0, 4, width(), 4);
    }
};

QLabel* roleLabel(const QString& text, const char* role, QWidget* parent = nullptr)
{
    auto* label = new QLabel(text, parent);
    label->setProperty("uiRole", role);
    return label;
}

} // namespace

TicketWindow::TicketWindow(UserSession* session,
                           LocalApi* api,
                           LocalImages* images,
                           ChangeNotifier* notifier,
                           QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::TicketWindow)
    , m_session(session)
    , m_api(api)
    , m_images(images)
    , m_notifier(notifier)
    , m_model(nullptr)
    , m_proxy(nullptr)
    , m_selectedTicketId(-1)
    , m_printManager(nullptr)
{
    ui->setupUi(this);
    m_printManager = new TicketPrintManager(m_api, m_images, this);

    // Setup notification system
    m_bell = new NotificationBell(m_api, ui->notificationBtn,
                                  m_session ? m_session->username() : QString(),
                                  m_notifier, this);



    setupUI();
    setupTable();
    buildLayout();
    loadTickets(QDate::currentDate());
    showMaximized();
}

TicketWindow::~TicketWindow()
{
    delete ui;
}

void TicketWindow::setupUI()
{
    setWindowTitle(tr("Ticket History"));

    // Logout button (icon set via StyleManager)
    ui->logoutBtn->setText("");
    ui->logoutBtn->setIcon(QIcon(StyleManager::iconPath("logout")));
    ui->logoutBtn->setIconSize(QSize(22, 22));
    ui->logoutBtn->setFixedSize(36, 36);
    ui->logoutBtn->setToolTip(tr("Logout"));

    // Notification button — icon only, square
    ui->notificationBtn->setText("");
    ui->notificationBtn->setIcon(QIcon(StyleManager::iconPath("notification")));
    ui->notificationBtn->setIconSize(QSize(22, 22));
    ui->notificationBtn->setFixedSize(36, 36);
    ui->notificationBtn->setToolTip(tr("Notifications"));

    // Action buttons
    ui->printTicketBtn->setText(tr("Print"));
    ui->editTicketBtn->setText(tr("Edit"));
    ui->refundTicketBtn->setText(tr("Refund"));
    ui->deleteTicketBtn->setText(tr("Delete"));

    ui->printTicketBtn->setEnabled(false);
    ui->editTicketBtn->setEnabled(false);
    ui->refundTicketBtn->setEnabled(false);
    ui->deleteTicketBtn->setEnabled(false);

    // Search bar
    ui->searchBar->setPlaceholderText(tr("Search by ticket #, seller, date, or status..."));

    // Date filter — default to today
    ui->dateSelect->setDate(QDate::currentDate());
    ui->dateSelect->setCalendarPopup(true);
    ui->dateSelect->setDisplayFormat("d MMM yyyy");

    updateUserDisplay();

    // Income label
    if (ui->incomeLabel) {
        ui->incomeLabel->setObjectName("incomeLabel");
        ui->incomeLabel->setAlignment(Qt::AlignCenter);
    }

    // Hide delete button if user lacks permission
    if (m_session) {
        bool canDelete = Permissions::can(
            m_api, m_session->username(), m_session->role(),
            AccessControl::PERMISSION_DELETE_TICKET);
        ui->deleteTicketBtn->setVisible(canDelete);
    } else {
        ui->deleteTicketBtn->setVisible(false);
    }

    connect(ui->logoutBtn,       &QPushButton::clicked, this, &TicketWindow::onLogoutClicked);
    connect(ui->notificationBtn, &QPushButton::clicked, m_bell, &NotificationBell::togglePanel);
    connect(ui->dateSelect,      &QDateEdit::dateChanged, this, &TicketWindow::onDateChanged);
    connect(ui->searchBar,       &QLineEdit::textChanged, this, &TicketWindow::onSearchChanged);
    connect(ui->deleteTicketBtn, &QPushButton::clicked, this, &TicketWindow::onDeleteClicked);
    connect(ui->refundTicketBtn, &QPushButton::clicked, this, &TicketWindow::onRefundClicked);
    connect(ui->editTicketBtn,   &QPushButton::clicked, this, &TicketWindow::onEditClicked);
    connect(ui->printTicketBtn,  &QPushButton::clicked, this, &TicketWindow::onPrintClicked);

    if (m_notifier) {
        connect(m_notifier, &ChangeNotifier::ticketUpdated, this, [this](int) {
            AppHelpers::bringToFront(this);
            m_bell->refreshNow();
            loadTickets(ui->dateSelect->date());
        });
        connect(m_notifier, &ChangeNotifier::ticketCreated, this, [this](int) {
            m_bell->refreshNow();
            loadTickets(ui->dateSelect->date());
        });
        connect(m_notifier, &ChangeNotifier::appNavigateRequested, this, [this](const QString&) {
            AppHelpers::bringToFront(this);
        });
    }

    // Trailing stretch keeps cart items packed at the top
    ui->ticketCartItems->addStretch(1);
}

void TicketWindow::setupTable()
{
    m_model = new QStandardItemModel(0, NUM_COLUMNS, this);
    m_model->setHorizontalHeaderLabels({ tr("Ticket #"), tr("Date"), tr("Time"), tr("Seller"), tr("Total"), tr("Discount"), tr("Status")});

    m_proxy = new TicketFilterProxy(this);
    m_proxy->setSourceModel(m_model);
    m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxy->setFilterKeyColumn(-1); // search all columns

    ui->ticketList->setModel(m_proxy);
    ui->ticketList->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->ticketList->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->ticketList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->ticketList->setAlternatingRowColors(false);
    ui->ticketList->setShowGrid(false);
    ui->ticketList->setWordWrap(false);
    ui->ticketList->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    ui->ticketList->verticalHeader()->setDefaultSectionSize(50);
    ui->ticketList->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    ui->ticketList->setItemDelegateForColumn(COL_SELLER, new AvatarDelegate(ui->ticketList));
    auto* statusPills = new PillDelegate(ui->ticketList);
    statusPills->addValue("COMPLETED", PillDelegate::Tone::Green, tr("Completed"));
    statusPills->addValue("REFUNDED", PillDelegate::Tone::Red, tr("Refunded"));
    statusPills->addValue("CANCELLED", PillDelegate::Tone::Neutral, tr("Cancelled"));
    ui->ticketList->setItemDelegateForColumn(COL_STATUS, statusPills);
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
    if (!m_api || !m_model) return;

    m_model->removeRows(0, m_model->rowCount());

    // Day list: all tickets for the selected date, newest time first.
    // OPEN tickets are excluded.
    const QList<DataRow> rows = m_api->select(
        "tickets", {{"date", date.toString("yyyy-MM-dd")}}, "time DESC");

    if (!m_api->ok()) {
        qWarning() << "TicketWindow: failed to load tickets:" << m_api->lastError();
        return;
    }

    double income = 0.0;
    int ticketCount = 0;
    int refundedCount = 0;

    for (const DataRow& t : rows) {
        const QString status = t.value("status").toString();
        if (status == "OPEN")
            continue;

        int     id         = t.value("id").toInt();
        QString ticketNum  = t.value("ticket_number").toString();
        QString dateStr    = t.value("date").toString();
        QString timeStr    = t.value("time").toString();
        QString seller     = t.value("seller_display_name").toString();
        double  total      = t.value("total").toDouble();
        double  discount   = t.value("discount_amount").toDouble();

        if (status == "COMPLETED") {
            income += total;
        }
        ++ticketCount;
        if (status == "REFUNDED")
            ++refundedCount;

        QList<QStandardItem*> row;

        auto item = [](const QString& text) {
            QStandardItem* i = new QStandardItem(text);
            i->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            return i;
        };

        QStandardItem* numItem = item(ticketNum);
        numItem->setData(id, Qt::UserRole); // store DB id for later use
        numItem->setData(t.value("subtotal").toDouble(), ROLE_SUBTOTAL);

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

    if (ui->incomeLabel) {
        ui->incomeLabel->setText(tr("Income: %1").arg(AppHelpers::formatPrice(income)));
    }
    m_ticketCountLabel->setText(ticketCount == 1 ? tr("1 ticket")
                                                 : tr("%1 tickets").arg(ticketCount));
    m_refundedCountLabel->setText(tr("%1 refunded").arg(refundedCount));
    m_refundedCountLabel->setVisible(refundedCount > 0);
}

void TicketWindow::updateUserDisplay()
{
    if (m_session) {
        ui->userLabel->setText(
            QString("%1 (%2)").arg(m_session->displayName()).arg(AppHelpers::roleName(m_session->role())));
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
    if (!LogoutManager::confirmAndLogout(this, m_session->sessionId())) {
        return;
    }
    close();
    emit logoutRequested();
}

void TicketWindow::onTicketSelected(const QModelIndex& current, const QModelIndex& /*previous*/)
{
    Animations::fadeChange(m_receiptPaper);

    if (!current.isValid()) {
        clearTicketCart();
        setActionButtonsEnabled(false);
        m_selectedTicketId = -1;
        showReceiptPlaceholder();
        return;
    }

    // Map proxy index back to source to get the stored ticket id
    QModelIndex sourceIndex = m_proxy->mapToSource(current);
    QStandardItem* numItem  = m_model->item(sourceIndex.row(), COL_TICKET_NUM);
    if (!numItem) return;

    m_selectedTicketId = numItem->data(Qt::UserRole).toInt();
    showReceiptFor(sourceIndex.row());
    loadTicketItems(m_selectedTicketId);
    setActionButtonsEnabled(true);
}

void TicketWindow::loadTicketItems(int ticketId)
{
    clearTicketCart();
    if (!m_api || ticketId <= 0) return;

    const QList<DataRow> rows = m_api->select(
        "ticket_items", {{"ticket_id", ticketId}}, "id");

    if (!m_api->ok()) {
        qWarning() << "TicketWindow: failed to load ticket items:" << m_api->lastError();
        return;
    }

    for (const DataRow& it : rows) {
        QString name     = it.value("product_name").toString();
        QString varName  = it.value("variant_name").toString();
        int     qty      = it.value("quantity").toInt();
        double  total    = it.value("total_price").toDouble();

        // Build display name — append variant if present
        QString displayName = varName.isEmpty() ? name : QString("%1 — %2").arg(name, varName);

        QWidget* row = new QWidget();
        row->setObjectName("ticket-cart-item");

        QHBoxLayout* layout = new QHBoxLayout(row);
        layout->setContentsMargins(0, 2, 0, 2);
        layout->setSpacing(8);

        QLabel* nameLabel = new QLabel(QString("%1 × %2").arg(qty).arg(displayName), row);
        nameLabel->setObjectName("ticket-cart-item-name");
        nameLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        nameLabel->setWordWrap(true);

        QLabel* priceLabel = new QLabel(QString::number(total, 'f', AppConstants::PRICE_DECIMALS), row);
        priceLabel->setObjectName("ticket-cart-item-price");
        priceLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        layout->addWidget(nameLabel, 1);
        layout->addWidget(priceLabel, 0);

        // Insert before the trailing stretch
        ui->ticketCartItems->insertWidget(ui->ticketCartItems->count() - 1, row);
    }

    // Row sizes are only known once they are styled and laid out.
    QTimer::singleShot(0, this, [this]() { fitReceiptList(); });
}

void TicketWindow::fitReceiptList()
{
    // The list takes the room it needs and scrolls only for very long tickets.
    constexpr int MaxListHeight = 300;
    QLayout* layout = ui->ticketCart->widget()->layout();
    layout->invalidate();
    const int width = ui->ticketCart->viewport()->width();
    const int height = layout->hasHeightForWidth() ? layout->totalHeightForWidth(width)
                                                   : layout->totalSizeHint().height();
    ui->ticketCart->setFixedHeight(qMin(height + 2, MaxListHeight));
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

    if (m_session && !Permissions::can(
            m_api, m_session->username(), m_session->role(),
            AccessControl::PERMISSION_DELETE_TICKET)) {
        AppHelpers::showWarning(this, tr("You do not have permission to delete tickets."));
        return;
    }

    if (!AppHelpers::confirmAction(this, tr("Are you sure you want to permanently delete this ticket?\nThis action cannot be undone.")))
        return;

    // One transaction: restores stock only if the ticket still moved it
    // (COMPLETED), then deletes header + items in one transaction.
    if (!m_api->deleteTicketWithStock(m_selectedTicketId)) {
        qWarning() << "Failed to delete ticket:" << m_api->lastError();
        AppHelpers::showError(this, tr("Failed to delete ticket.\n\n") + m_api->lastError());
        return;
    }

    m_selectedTicketId = -1;
    clearTicketCart();
    showReceiptPlaceholder();
    setActionButtonsEnabled(false);
    loadTickets(ui->dateSelect->date());

    // Notify Register to refresh stock and income in case items were involved
    if (m_notifier) {
        m_notifier->notifyChange(ChangeType::PRODUCT_CHANGED, 0);
        m_notifier->notifyChange(ChangeType::TICKET_UPDATED, 0);
    }
}

void TicketWindow::onRefundClicked()
{
    if (m_selectedTicketId <= 0) return;

    if (!AppHelpers::confirmAction(this, tr("Mark this ticket as refunded and restore stock?")))
        return;

    // One transaction: COMPLETED -> REFUNDED + stock restore. A ticket can
    // only be refunded once.
    if (!m_api->refundTicket(m_selectedTicketId)) {
        qWarning() << "Failed to mark ticket as refunded:" << m_api->lastError();
        AppHelpers::showError(this, tr("Failed to mark ticket as refunded.\n\n") + m_api->lastError());
        return;
    }

    // Notify Register to refresh product stock and income display
    if (m_notifier) {
        m_notifier->notifyChange(ChangeType::PRODUCT_CHANGED, 0);
        m_notifier->notifyChange(ChangeType::TICKET_UPDATED, 0);
    }

    // Refresh the table and keep the same date
    loadTickets(ui->dateSelect->date());

    // Clear cart since status changed
    clearTicketCart();
    showReceiptPlaceholder();
    setActionButtonsEnabled(false);
    m_selectedTicketId = -1;
}

void TicketWindow::onEditClicked()
{
    if (m_selectedTicketId <= 0) return;
    if (!m_session) return;

    // Route through the Launcher — it opens/targets Register with the current
    // session. No intermediate file is needed anymore.
    if (m_notifier) {
        QString request = QString("Register|open-ticket|%1").arg(m_selectedTicketId);
        m_notifier->notifyChange(ChangeType::APP_OPEN_REQUESTED, m_selectedTicketId, request);
    }
}

void TicketWindow::onPrintClicked()
{
    if (m_selectedTicketId <= 0) return;

    // ── Check whether thermal printer is enabled ──────────────────────────────
    // Printer settings are per-machine (LocalSettings). If disabled,
    // go straight to the invoice preview.
    const bool printerEnabled = LocalSettings::load().printer.value("enabled", false).toBool();

    const DataRow ticketRow = m_api->ticket(m_selectedTicketId);
    const QString ticketTotal = AppHelpers::formatPrice(ticketRow.value("total").toDouble());

    // ── Printer disabled → straight to invoice, no choice dialog ─────────────
    if (!printerEnabled) {
        m_printManager->printTicketWithPreview(m_selectedTicketId);
        return;
    }

    // ── Printer enabled → show choice dialog ─────────────────────────────────
    ModernDialog choiceDialog(ModernDialog::Look::Band, "printer", ModernDialog::Tone::Accent,
                              tr("Print ticket #%1").arg(ticketRow.value("ticket_number").toString()),
                              ticketTotal, 420, this);

    // One large row per format; clicking it chooses straight away.
    const auto choiceRow = [&choiceDialog](const QString& icon, const QString& title, const QString& detail) {
        auto* row = new QPushButton(&choiceDialog);
        row->setObjectName("modalChoice");
        row->setAutoDefault(false);
        row->setCursor(Qt::PointingHandCursor);
        auto* layout = new QHBoxLayout(row);
        layout->setContentsMargins(12, 10, 12, 10);
        layout->setSpacing(12);
        auto* iconLabel = new QLabel(row);
        iconLabel->setObjectName("modalIconDot");
        iconLabel->setProperty("tone", "accent");
        iconLabel->setFixedSize(40, 40);
        iconLabel->setAlignment(Qt::AlignCenter);
        iconLabel->setPixmap(LineIcons::pixmap(icon, LineIcons::colors().accent, 22));
        auto* texts = new QVBoxLayout;
        texts->setSpacing(1);
        auto* titleLabel = new QLabel(title, row);
        titleLabel->setObjectName("modalChoiceTitle");
        auto* detailLabel = new QLabel(detail, row);
        detailLabel->setObjectName("modalSubtitle");
        texts->addWidget(titleLabel);
        texts->addWidget(detailLabel);
        auto* arrow = new QLabel(row);
        arrow->setPixmap(LineIcons::pixmap("chevr", LineIcons::colors().muted, 18));
        layout->addWidget(iconLabel);
        layout->addLayout(texts, 1);
        layout->addWidget(arrow);
        for (QLabel* label : {iconLabel, titleLabel, detailLabel, arrow})
            label->setAttribute(Qt::WA_TransparentForMouseEvents);
        choiceDialog.body()->addWidget(row);
        return row;
    };

    QPushButton* invoiceBtn = choiceRow("receipt", tr("Invoice"), tr("A4 page, to print or save as PDF"));
    QPushButton* ticketBtn  = choiceRow("printer", tr("Ticket"), tr("Receipt printer, printed right away"));
    choiceDialog.addCancelLink(tr("Cancel"));

    bool invoiceChosen = false;
    bool ticketChosen  = false;

    connect(invoiceBtn, &QPushButton::clicked, [&]() { invoiceChosen = true; choiceDialog.accept(); });
    connect(ticketBtn,  &QPushButton::clicked, [&]() { ticketChosen  = true; choiceDialog.accept(); });

    choiceDialog.exec();

    // ── Invoice ───────────────────────────────────────────────────────────────
    if (invoiceChosen) {
        m_printManager->printTicketWithPreview(m_selectedTicketId);
        return;
    }

    // ── Thermal ticket ────────────────────────────────────────────────────────
    if (!ticketChosen) return;

    const ThermalPrinter::TicketData data = Receipt::loadTicket(m_api, m_images, m_selectedTicketId);

    ThermalPrinter printer(LocalSettings::load().printer);
    if (!printer.print(data)) {
        AppHelpers::showError(this, printer.lastError());
    }
}

// ── Layout: title and KPI pills, toolbar, ticket list + receipt ─────────────

void TicketWindow::buildLayout()
{
    const LineIcons::Colors colors = LineIcons::colors();

    auto* root = new QWidget;
    root->setObjectName("ticketsRoot");
    auto* rootLayout = new QVBoxLayout(root);
    rootLayout->setContentsMargins(26, 18, 26, 18);
    rootLayout->setSpacing(14);

    // Header
    auto* header = new QHBoxLayout;
    header->setSpacing(10);
    auto* titles = new QVBoxLayout;
    titles->setSpacing(0);
    titles->addWidget(roleLabel(tr("Tickets"), "pageTitle"));
    titles->addWidget(roleLabel(tr("Sales history, refunds and reprints"), "pageSub"));
    header->addLayout(titles, 1);
    ui->userLabel->setProperty("uiRole", "desc");
    header->addWidget(ui->userLabel, 0, Qt::AlignVCenter);
    m_ticketCountLabel = new QLabel;
    m_ticketCountLabel->setObjectName("kpiPill");
    header->addWidget(m_ticketCountLabel, 0, Qt::AlignVCenter);
    header->addWidget(ui->incomeLabel, 0, Qt::AlignVCenter);
    m_refundedCountLabel = new QLabel;
    m_refundedCountLabel->setObjectName("kpiPillRed");
    header->addWidget(m_refundedCountLabel, 0, Qt::AlignVCenter);
    header->addSpacing(4);
    header->addWidget(ui->notificationBtn);
    header->addWidget(ui->logoutBtn);
    rootLayout->addLayout(header);

    // Toolbar: day, search, status
    auto* toolbar = new QHBoxLayout;
    toolbar->setSpacing(10);
    auto dayButton = [&](const QString& icon, int days, const QString& tip) {
        auto* button = new QPushButton;
        button->setObjectName("dateNavBtn");
        button->setIcon(LineIcons::icon(icon, colors.text, 16));
        button->setToolTip(tip);
        button->setCursor(Qt::PointingHandCursor);
        connect(button, &QPushButton::clicked, this, [this, days]() {
            ui->dateSelect->setDate(ui->dateSelect->date().addDays(days));
        });
        return button;
    };
    toolbar->addWidget(dayButton("chevl", -1, tr("Previous day")));
    ui->dateSelect->setMinimumWidth(150);
    toolbar->addWidget(ui->dateSelect);
    toolbar->addWidget(dayButton("chevr", 1, tr("Next day")));
    toolbar->addWidget(ui->searchBar, 1);

    auto* segment = new QWidget;
    segment->setObjectName("ticketsSeg");
    auto* segmentLayout = new QHBoxLayout(segment);
    segmentLayout->setContentsMargins(3, 3, 3, 3);
    segmentLayout->setSpacing(2);
    auto* segmentGroup = new QButtonGroup(this);
    const QList<QPair<QString, QString>> statuses = {
        {tr("All"), QString()}, {tr("Completed"), "COMPLETED"}, {tr("Refunded"), "REFUNDED"}};
    for (const auto& status : statuses) {
        auto* button = new QPushButton(status.first);
        button->setObjectName("ticketsSegItem");
        button->setCheckable(true);
        button->setChecked(status.second.isEmpty());
        button->setCursor(Qt::PointingHandCursor);
        segmentGroup->addButton(button);
        segmentLayout->addWidget(button);
        const QString value = status.second;
        connect(button, &QPushButton::clicked, this, [this, value]() {
            static_cast<TicketFilterProxy*>(m_proxy)->setStatus(value);
        });
    }
    toolbar->addWidget(segment);
    rootLayout->addLayout(toolbar);

    // Main area
    auto* main = new QHBoxLayout;
    main->setSpacing(16);

    auto* listCard = new QFrame;
    listCard->setProperty("uiCard", true);
    auto* listLayout = new QVBoxLayout(listCard);
    listLayout->setContentsMargins(12, 12, 12, 10);
    listLayout->setSpacing(8);
    listLayout->addWidget(ui->ticketList, 1);
    listLayout->addWidget(roleLabel(tr("Click a ticket to see its receipt"), "desc"));
    main->addWidget(listCard, 1);

    constexpr int ReceiptWidth = 372;
    auto* side = new QVBoxLayout;
    side->setContentsMargins(0, 0, 0, 0);
    side->setSpacing(12);
    m_receiptPaper = new ReceiptPaper;
    m_receiptPaper->setFixedWidth(ReceiptWidth);
    auto* paper = new QVBoxLayout(m_receiptPaper);
    paper->setContentsMargins(22, 18, 22, 24);
    paper->setSpacing(4);

    m_receiptPlaceholder = roleLabel(tr("Select a ticket to see its receipt"), "desc");
    m_receiptPlaceholder->setAlignment(Qt::AlignCenter);
    m_receiptPlaceholder->setMinimumHeight(180);
    paper->addWidget(m_receiptPlaceholder);

    m_receiptBody = new QWidget;
    auto* body = new QVBoxLayout(m_receiptBody);
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(4);
    auto centered = [&](QLabel* label) {
        label->setAlignment(Qt::AlignCenter);
        label->setWordWrap(true);
        body->addWidget(label);
        return label;
    };
    m_receiptShopName = centered(roleLabel(QString(), "cardTitle"));
    m_receiptShopAddress = centered(roleLabel(QString(), "desc"));
    body->addWidget(new DashedLine);
    auto* numberRow = new QHBoxLayout;
    m_receiptNumber = roleLabel(QString(), "rowTitle");
    m_receiptNumber->setWordWrap(true);
    m_receiptTime = roleLabel(QString(), "desc");
    numberRow->addWidget(m_receiptNumber, 1);
    numberRow->addWidget(m_receiptTime);
    body->addLayout(numberRow);
    m_receiptSeller = roleLabel(QString(), "desc");
    body->addWidget(m_receiptSeller);
    body->addWidget(new DashedLine);
    body->addWidget(ui->ticketCart);
    body->addWidget(new DashedLine);
    auto totalsRow = [&](const QString& title, QLabel*& value, const char* role) {
        auto* row = new QWidget;
        auto* layout = new QHBoxLayout(row);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(roleLabel(title, role));
        layout->addStretch();
        value = roleLabel(QString(), role);
        layout->addWidget(value);
        body->addWidget(row);
        return row;
    };
    totalsRow(tr("Subtotal"), m_receiptSubtotal, "desc");
    m_receiptDiscountRow = totalsRow(tr("Discount"), m_receiptDiscount, "desc");
    totalsRow("TOTAL", m_receiptTotal, "cardTitle");
    paper->addWidget(m_receiptBody);

    side->addWidget(m_receiptPaper);
    side->addStretch();

    auto* actions = new QHBoxLayout;
    actions->setSpacing(8);
    ui->printTicketBtn->setIcon(LineIcons::icon("printer", Qt::white, 16));
    ui->printTicketBtn->setProperty("btnStyle", "edit");
    ui->editTicketBtn->setIcon(LineIcons::icon("edit", colors.text, 16));
    ui->refundTicketBtn->setIcon(LineIcons::icon("undo", colors.text, 16));
    ui->deleteTicketBtn->setIcon(LineIcons::icon("trash", Qt::white, 16));
    ui->deleteTicketBtn->setProperty("btnStyle", "delete");
    actions->addWidget(ui->printTicketBtn, 1);
    actions->addWidget(ui->editTicketBtn);
    actions->addWidget(ui->refundTicketBtn);
    actions->addWidget(ui->deleteTicketBtn);
    side->addLayout(actions);

    auto* sideWidget = new QWidget;
    sideWidget->setObjectName("ticketsSide");
    sideWidget->setLayout(side);
    sideWidget->setFixedWidth(ReceiptWidth);
    main->addWidget(sideWidget);
    rootLayout->addLayout(main, 1);

    // The designer layout stays hidden: every control now lives in the new one.
    QWidget* designed = takeCentralWidget();
    designed->setParent(this);
    designed->hide();
    root->setObjectName("centralwidget");
    setCentralWidget(root);

    showReceiptPlaceholder();
}

void TicketWindow::showReceiptPlaceholder()
{
    m_receiptBody->hide();
    m_receiptPlaceholder->show();
}

void TicketWindow::showReceiptFor(int sourceRow)
{
    if (!m_shopLoaded) {
        const DataRow shop = m_api->profile();
        if (!shop.isEmpty()) {
            m_shopLoaded = true;
            m_receiptShopName->setText(shop.value("shop_name").toString());
            QStringList address;
            for (const char* key : {"address", "city"}) {
                const QString part = shop.value(key).toString().trimmed();
                if (!part.isEmpty())
                    address << part;
            }
            m_receiptShopAddress->setText(address.join(", "));
            m_receiptShopAddress->setVisible(!address.isEmpty());
        }
    }

    const auto text = [&](int column) { return m_model->item(sourceRow, column)->text(); };
    const double subtotal = m_model->item(sourceRow, COL_TICKET_NUM)->data(ROLE_SUBTOTAL).toDouble();
    m_receiptNumber->setText(tr("Ticket #") + text(COL_TICKET_NUM));
    m_receiptTime->setText(text(COL_TIME));
    m_receiptSeller->setText(tr("Seller: ") + text(COL_SELLER));
    m_receiptSubtotal->setText(AppHelpers::formatPrice(subtotal));
    m_receiptDiscount->setText("-" + text(COL_DISCOUNT));
    m_receiptDiscountRow->setVisible(text(COL_DISCOUNT) != QLatin1String("-"));
    m_receiptTotal->setText(text(COL_TOTAL));

    m_receiptPlaceholder->hide();
    m_receiptBody->show();
}
