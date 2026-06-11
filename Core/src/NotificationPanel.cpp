#include "NotificationPanel.h"
#include "NotificationManager.h"
#include "StyleManager.h"
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

NotificationPanel::NotificationPanel(NotificationManager* manager, const QString& username, const QSqlDatabase& db, QWidget* parent)
    : QFrame(parent)
    , m_manager(manager)
    , m_username(username)
    , m_db(db)
    , m_contentLayout(nullptr)
{
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
    setObjectName("notificationPanel");
    setMinimumWidth(350);
    setMaximumWidth(400);
    
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);
    
    QWidget* header = new QWidget(this);
    header->setObjectName("notifHeader");
    header->setFixedHeight(40);
    QHBoxLayout* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(12, 0, 12, 0);
    headerLayout->setSpacing(8);
    
    QLabel* title = new QLabel(tr("Notifications"), header);
    
    QPushButton* refreshBtn = new QPushButton(header);
    refreshBtn->setIcon(QIcon(StyleManager::iconPath("refresh")));
    refreshBtn->setIconSize(QSize(18, 18));
    refreshBtn->setFixedSize(26, 26);
    refreshBtn->setToolTip(tr("Refresh notifications"));
    connect(refreshBtn, &QPushButton::clicked, this, &NotificationPanel::onRefreshClicked);
    
    QPushButton* markAllBtn = new QPushButton(tr("Mark all read"), header);
    connect(markAllBtn, &QPushButton::clicked, this, &NotificationPanel::onMarkAllReadClicked);
    
    headerLayout->addWidget(title);
    headerLayout->addStretch();
    headerLayout->addWidget(refreshBtn);
    headerLayout->addWidget(markAllBtn);
    
    m_mainLayout->addWidget(header);
    
    QScrollArea* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    
    m_contentWidget = new QWidget(scrollArea);
    m_contentWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    m_contentLayout = new QVBoxLayout(m_contentWidget);
    m_contentLayout->setContentsMargins(8, 8, 8, 8);
    m_contentLayout->setSpacing(4);
    m_contentLayout->addStretch();
    
    m_emptyLabel = new QLabel(tr("No notifications"), m_contentWidget);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setMinimumHeight(60);
    m_contentLayout->insertWidget(0, m_emptyLabel);
    
    scrollArea->setWidget(m_contentWidget);
    m_mainLayout->addWidget(scrollArea, 1);
    
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    setMinimumHeight(80);
    setMaximumHeight(400);
    
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

void NotificationPanel::refreshNotifications()
{
    QLayoutItem* item;
    while ((item = m_contentLayout->takeAt(0)) != nullptr) {
        if (QWidget* w = item->widget()) {
            if (w != m_emptyLabel) {
                w->deleteLater();
            }
        }
        delete item;
    }
    
    QList<NotificationManager::Notification> notifications = m_manager->getAll();
    
    if (notifications.isEmpty()) {
        m_emptyLabel->setVisible(true);
    } else {
        m_emptyLabel->setVisible(false);
        for (const auto& notif : notifications) {
            addNotificationItem(notif);
        }
    }
    
    m_contentLayout->addStretch();
}

void NotificationPanel::addNotificationItem(const NotificationManager::Notification& notif)
{
    QFrame* item = new QFrame(m_contentWidget);
    item->setObjectName("notifItem");
    item->setCursor(Qt::PointingHandCursor);
    
    QVBoxLayout* itemLayout = new QVBoxLayout(item);
    itemLayout->setContentsMargins(12, 10, 12, 10);
    itemLayout->setSpacing(4);
    
    QHBoxLayout* titleRow = new QHBoxLayout();
    titleRow->setSpacing(8);
    
    if (!notif.read) {
        QLabel* dot = new QLabel(item);
        dot->setObjectName("notifDot");
        dot->setFixedSize(8, 8);
        titleRow->addWidget(dot);
    }
    
    QLabel* titleLabel = new QLabel(notif.title, item);
    titleLabel->setObjectName("notifTitle");
    
    QLabel* dateLabel = new QLabel(notif.date, item);
    dateLabel->setObjectName("notifDate");
    
    titleRow->addWidget(titleLabel);
    titleRow->addStretch();
    titleRow->addWidget(dateLabel);
    
    QLabel* msgLabel = new QLabel(notif.message, item);
    msgLabel->setObjectName("notifMessage");
    msgLabel->setWordWrap(true);
    
    itemLayout->addLayout(titleRow);
    itemLayout->addWidget(msgLabel);
    
    item->setProperty("notif_id", notif.id);
    item->setProperty("notif_type", static_cast<int>(notif.type));
    item->setProperty("notif_context", notif.contextId);
    item->setAttribute(Qt::WA_Hover);
    item->installEventFilter(this);
    
    item->setContextMenuPolicy(Qt::NoContextMenu);
    
    m_contentLayout->insertWidget(m_contentLayout->count() - 1, item);
}

void NotificationPanel::onNotificationClicked(const QString& id, NotificationManager::Type type, int contextId)
{
    m_manager->markAsRead(id);
    navigateToApp(type, contextId);
    hide();
}

bool NotificationPanel::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease) {
        QFrame* frame = qobject_cast<QFrame*>(obj);
        
        if (frame && frame->property("notif_id").isValid()) {
            QString id = frame->property("notif_id").toString();
            int type = frame->property("notif_type").toInt();
            int contextId = frame->property("notif_context").toInt();
            
            m_manager->markAsRead(id);
            navigateToApp(static_cast<NotificationManager::Type>(type), contextId);
            
            hide();
            return true;
        }
    }
    return QFrame::eventFilter(obj, event);
}

void NotificationPanel::navigateToApp(NotificationManager::Type type, int contextId)
{
    QString appPath;
    QStringList args;
    
    QString appDir = QCoreApplication::applicationDirPath();
    QString parentDir = QFileInfo(appDir).absolutePath();
    
    // Pass username to target app
    args << "--user" << m_username;
    
    switch (type) {
        case NotificationManager::Type::OpenTicket:
            appPath = parentDir + "/Register/Register";
            args << "--open-ticket" << QString::number(contextId);
            break;
        case NotificationManager::Type::ExpenseDue:
        case NotificationManager::Type::ExpenseOverdue:
            appPath = parentDir + "/Financial/Financial";
            args << "--expense" << QString::number(contextId);
            break;
        case NotificationManager::Type::OutOfStock:
        case NotificationManager::Type::NearlyOutOfStock:
            appPath = parentDir + "/Inventory/Inventory";
            args << "--product" << QString::number(contextId);
            break;
    }
    
    if (!appPath.isEmpty()) {
        QProcess::startDetached(appPath, args);
    }
}

void NotificationPanel::showEvent(QShowEvent* event)
{
    QFrame::showEvent(event);
    if (parentWidget()) {
        setMaximumHeight(parentWidget()->height() / 2);
    }
    refreshNotifications();
}
