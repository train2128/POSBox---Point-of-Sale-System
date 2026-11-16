#include "NotificationPanel.h"
#include "Animations.h"
#include "NotificationManager.h"
#include "ChangeNotifier.h"
#include "LineIcons.h"
#include "ModernDialog.h"
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QFrame>
#include <QMouseEvent>
#include <QProcess>
#include <QCoreApplication>
#include <QFileInfo>
#include <QDate>
#include <QLocale>
#include <QPainter>
#include <QStyle>
#include <QStyleOption>

namespace {

constexpr int PanelWidth = 420;
constexpr int MaxListHeight = 460;
constexpr int EmptyListHeight = 150;

// Stock problems and overdue bills come first, whatever else is waiting.
bool needsAttention(const NotificationManager::Notification& notif)
{
    using Type = NotificationManager::Type;
    return notif.priority == QLatin1String("high") || notif.type == Type::OutOfStock
        || notif.type == Type::NearlyOutOfStock || notif.type == Type::ExpenseOverdue;
}

const char* stripeTone(NotificationManager::Type type)
{
    using Type = NotificationManager::Type;
    switch (type) {
    case Type::OutOfStock:
    case Type::ExpenseOverdue:   return "red";
    case Type::NearlyOutOfStock:
    case Type::ExpenseDue:       return "amber";
    default:                     return "accent";
    }
}

QString friendlyDate(const QString& text)
{
    const QDate date = QDate::fromString(text, "yyyy-MM-dd");
    if (!date.isValid())
        return text;
    const QDate today = QDate::currentDate();
    if (date == today)
        return QCoreApplication::translate("NotificationPanel", "Today");
    if (date == today.addDays(-1))
        return QCoreApplication::translate("NotificationPanel", "Yesterday");
    const QLocale english(QLocale::English);
    return english.toString(date, date.year() == today.year() ? "d MMM" : "d MMM yyyy");
}

void repolish(QWidget* widget)
{
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
}

} // namespace

NotificationPanel::NotificationPanel(NotificationManager* manager,
                                     const QString& username,
                                     ChangeNotifier* notifier,
                                     QWidget* parent)
    : QFrame(parent)
    , m_manager(manager)
    , m_notifier(notifier)
    , m_username(username)
    , m_contentLayout(nullptr)
{
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    setObjectName("notificationPanel");
    setFixedWidth(PanelWidth);
    setAttribute(Qt::WA_StyledBackground, true);
    setAttribute(Qt::WA_TranslucentBackground);

    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(1, 1, 1, 1);
    m_mainLayout->setSpacing(0);

    m_header = new QWidget(this);
    m_header->setObjectName("notifHeader");
    m_header->setAttribute(Qt::WA_StyledBackground, true);
    QHBoxLayout* headerLayout = new QHBoxLayout(m_header);
    headerLayout->setContentsMargins(14, 12, 12, 12);
    headerLayout->setSpacing(8);

    m_bell = new QLabel(m_header);
    m_bell->setObjectName("notifBell");
    m_bell->setFixedSize(36, 36);
    m_bell->setAlignment(Qt::AlignCenter);
    m_bell->setPixmap(LineIcons::pixmap("bell", Qt::white, 20));

    QVBoxLayout* titles = new QVBoxLayout;
    titles->setSpacing(0);
    QLabel* title = new QLabel(tr("Notifications"), m_header);
    title->setObjectName("notifHeaderTitle");
    m_subtitle = new QLabel(m_header);
    m_subtitle->setObjectName("notifSubtitle");
    titles->addStretch(1);
    titles->addWidget(title);
    titles->addWidget(m_subtitle);
    titles->addStretch(1);

    QPushButton* refreshBtn = new QPushButton(m_header);
    refreshBtn->setObjectName("notifRefresh");
    refreshBtn->setIcon(LineIcons::icon("refresh", LineIcons::colors().muted, 16));
    refreshBtn->setIconSize(QSize(16, 16));
    refreshBtn->setFixedSize(30, 30);
    refreshBtn->setToolTip(tr("Refresh notifications"));
    connect(refreshBtn, &QPushButton::clicked, this, &NotificationPanel::onRefreshClicked);

    m_markAllBtn = new QPushButton(tr("Read all"), m_header);
    m_markAllBtn->setObjectName("notifMarkAll");
    m_markAllBtn->setCursor(Qt::PointingHandCursor);
    m_markAllBtn->setToolTip(tr("Mark every notification as read"));
    connect(m_markAllBtn, &QPushButton::clicked, this, &NotificationPanel::onMarkAllReadClicked);

    // Dismisses everything for this user: it stays hidden until
    // a condition actually changes (which re-alerts).
    m_clearAllBtn = new QPushButton(tr("Clear all"), m_header);
    m_clearAllBtn->setObjectName("notifClearAll");
    m_clearAllBtn->setCursor(Qt::PointingHandCursor);
    m_clearAllBtn->setToolTip(tr("Hide all notifications until something changes"));
    connect(m_clearAllBtn, &QPushButton::clicked, this, &NotificationPanel::onClearAllClicked);

    headerLayout->addWidget(m_bell);
    headerLayout->addLayout(titles, 1);
    headerLayout->addWidget(refreshBtn);
    headerLayout->addWidget(m_markAllBtn);
    headerLayout->addWidget(m_clearAllBtn);

    m_mainLayout->addWidget(m_header);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->viewport()->setAutoFillBackground(false);

    m_contentWidget = new QWidget(m_scrollArea);
    m_contentWidget->setObjectName("notifContent");
    m_contentWidget->setAutoFillBackground(false);
    m_contentLayout = new QVBoxLayout(m_contentWidget);
    m_contentLayout->setContentsMargins(10, 10, 10, 10);
    m_contentLayout->setSpacing(6);

    m_emptyState = new QWidget(m_contentWidget);
    QVBoxLayout* emptyLayout = new QVBoxLayout(m_emptyState);
    emptyLayout->setContentsMargins(0, 14, 0, 14);
    emptyLayout->setSpacing(4);
    QLabel* emptyIcon = new QLabel(m_emptyState);
    emptyIcon->setObjectName("notifEmptyIcon");
    emptyIcon->setFixedSize(60, 60);
    emptyIcon->setAlignment(Qt::AlignCenter);
    emptyIcon->setPixmap(LineIcons::pixmap("check", ModernDialog::toneColor(ModernDialog::Tone::Green), 32));
    QLabel* emptyTitle = new QLabel(tr("You're all caught up"), m_emptyState);
    emptyTitle->setObjectName("notifEmptyTitle");
    emptyTitle->setAlignment(Qt::AlignCenter);
    QLabel* emptyHint = new QLabel(tr("New alerts will show up here"), m_emptyState);
    emptyHint->setObjectName("notifEmptyHint");
    emptyHint->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(emptyIcon, 0, Qt::AlignHCenter);
    emptyLayout->addSpacing(4);
    emptyLayout->addWidget(emptyTitle);
    emptyLayout->addWidget(emptyHint);

    m_scrollArea->setWidget(m_contentWidget);
    m_mainLayout->addWidget(m_scrollArea);

    connect(m_manager, &NotificationManager::notificationsChanged, this, &NotificationPanel::refreshNotifications);

    refreshNotifications();
}

NotificationPanel::~NotificationPanel()
{
}

void NotificationPanel::onRefreshClicked()
{
    m_manager->refreshNow();
}

void NotificationPanel::onMarkAllReadClicked()
{
    m_manager->markAllAsRead();
}

void NotificationPanel::onClearAllClicked()
{
    m_manager->dismissAll();
    hide();
}

void NotificationPanel::refreshNotifications()
{
    QLayoutItem* item;
    while ((item = m_contentLayout->takeAt(0)) != nullptr) {
        if (QWidget* w = item->widget()) {
            if (w != m_emptyState) {
                w->hide();
                w->deleteLater();
            }
        }
        delete item;
    }

    const QList<NotificationManager::Notification> notifications = m_manager->getAll();
    const bool empty = notifications.isEmpty();

    // The empty panel is plain; the colored band only appears with alerts.
    m_header->setProperty("empty", empty);
    repolish(m_header);
    m_bell->setVisible(!empty);
    m_subtitle->setVisible(!empty);
    m_markAllBtn->setVisible(!empty);
    m_clearAllBtn->setVisible(!empty);
    m_emptyState->setVisible(empty);

    if (empty) {
        m_contentLayout->addWidget(m_emptyState);
        m_contentLayout->addStretch();
        m_scrollArea->setFixedHeight(EmptyListHeight);
    } else {
        int unread = 0;
        QList<NotificationManager::Notification> attention, reminders;
        for (const auto& notif : notifications) {
            unread += notif.read ? 0 : 1;
            (needsAttention(notif) ? attention : reminders) << notif;
        }
        m_subtitle->setText(unread > 0 ? tr("%1 unread").arg(unread) : tr("All read"));

        const auto addSection = [this](const QString& heading,
                                       const QList<NotificationManager::Notification>& list) {
            if (list.isEmpty())
                return;
            QLabel* label = new QLabel(heading, m_contentWidget);
            label->setObjectName("notifSection");
            label->show();
            m_contentLayout->addWidget(label);
            for (const auto& notif : list)
                addNotificationItem(notif);
        };
        addSection(tr("NEEDS ATTENTION"), attention);
        addSection("REMINDERS", reminders);
        m_contentLayout->addStretch();

        m_contentLayout->invalidate();
        const int contentWidth = PanelWidth - 2;
        const int wanted = m_contentLayout->hasHeightForWidth()
                               ? m_contentLayout->totalHeightForWidth(contentWidth)
                               : m_contentLayout->totalSizeHint().height();
        m_scrollArea->setFixedHeight(qMin(wanted + 2, MaxListHeight));
    }
    m_header->ensurePolished();
    setFixedHeight(m_header->sizeHint().height() + m_scrollArea->height() + 2);
}

void NotificationPanel::addNotificationItem(const NotificationManager::Notification& notif)
{
    QFrame* item = new QFrame(m_contentWidget);
    item->setObjectName("notifItem");
    item->setCursor(Qt::PointingHandCursor);

    QHBoxLayout* itemLayout = new QHBoxLayout(item);
    itemLayout->setContentsMargins(0, 0, 0, 0);
    itemLayout->setSpacing(0);

    QFrame* stripe = new QFrame(item);
    stripe->setObjectName("notifStripe");
    stripe->setProperty("tone", stripeTone(notif.type));
    stripe->setFixedWidth(5);
    itemLayout->addWidget(stripe);

    QVBoxLayout* texts = new QVBoxLayout;
    texts->setContentsMargins(12, 8, 12, 8);
    texts->setSpacing(3);

    QHBoxLayout* titleRow = new QHBoxLayout();
    titleRow->setSpacing(8);

    QLabel* titleLabel = new QLabel(notif.title, item);
    titleLabel->setObjectName("notifTitle");
    titleLabel->setProperty("unread", !notif.read);

    QLabel* dateLabel = new QLabel(friendlyDate(notif.date), item);
    dateLabel->setObjectName("notifDate");

    titleRow->addWidget(titleLabel);
    titleRow->addStretch();
    titleRow->addWidget(dateLabel);

    QHBoxLayout* messageRow = new QHBoxLayout();
    messageRow->setSpacing(8);
    QLabel* msgLabel = new QLabel(notif.message, item);
    msgLabel->setObjectName("notifMessage");
    msgLabel->setWordWrap(true);
    messageRow->addWidget(msgLabel, 1);
    if (!notif.read) {
        QLabel* dot = new QLabel(item);
        dot->setObjectName("notifDot");
        dot->setFixedSize(8, 8);
        messageRow->addWidget(dot, 0, Qt::AlignVCenter);
    }

    texts->addLayout(titleRow);
    texts->addLayout(messageRow);
    itemLayout->addLayout(texts, 1);

    item->setProperty("notif_id", notif.id);
    item->setProperty("notif_action", notif.action);
    item->setProperty("notif_context", notif.contextId);
    item->setProperty("notif_priority", notif.priority);
    item->setAttribute(Qt::WA_Hover);
    item->installEventFilter(this);

    item->setContextMenuPolicy(Qt::NoContextMenu);

    item->show();  // counts towards the list's height even while the panel is closed
    m_contentLayout->addWidget(item);
}

bool NotificationPanel::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonRelease) {
        QFrame* frame = qobject_cast<QFrame*>(obj);

        if (frame && frame->property("notif_id").isValid()) {
            QString id = frame->property("notif_id").toString();
            QString action = frame->property("notif_action").toString();
            int contextId = frame->property("notif_context").toInt();

            m_manager->markAsRead(id);
            navigateToApp(action, contextId);

            hide();
            return true;
        }
    }
    return QFrame::eventFilter(obj, event);
}

void NotificationPanel::navigateToApp(const QString& action, int contextId)
{
    if (m_notifier)
        m_notifier->notifyChange(ChangeType::APP_OPEN_REQUESTED, contextId, action);
}

// A translucent window doesn't get its stylesheet background on its own.
void NotificationPanel::paintEvent(QPaintEvent*)
{
    QStyleOption option;
    option.initFrom(this);
    QPainter painter(this);
    style()->drawPrimitive(QStyle::PE_Widget, &option, &painter, this);
}

void NotificationPanel::showEvent(QShowEvent* event)
{
    QFrame::showEvent(event);

    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor("#2a2a2a"));
    pal.setColor(QPalette::Base, QColor("#2a2a2a"));
    pal.setColor(QPalette::Button, QColor("#2a2a2a"));
    setPalette(pal);

    if (parentWidget()) {
        setMaximumHeight(parentWidget()->height() / 2);
    }
    refreshNotifications();
    Animations::slideDownReveal(this);
}