#include "ChangeNotifier.h"
#include <QDebug>
#include <cstring>

ChangeNotifier::ChangeNotifier(QObject* parent)
    : QObject(parent)
    , m_sharedMemory(SHARED_MEMORY_KEY)
    , m_lastCheckTimestamp(0)
{
    initialize();
}

ChangeNotifier::~ChangeNotifier()
{
    stopPolling();
    if (m_sharedMemory.isAttached()) {
        m_sharedMemory.detach();
    }
}

bool ChangeNotifier::initialize()
{
    // Try to attach to existing shared memory
    if (m_sharedMemory.attach()) {
        qDebug() << "Attached to existing shared memory";
        return true;
    }

    // Create new shared memory
    int size = sizeof(int) + (sizeof(ChangeNotification) * MAX_NOTIFICATIONS);
    if (!m_sharedMemory.create(size)) {
        qWarning() << "Failed to create shared memory:" << m_sharedMemory.errorString();
        return false;
    }

    // Initialize the buffer
    m_sharedMemory.lock();
    memset(m_sharedMemory.data(), 0, size);
    m_sharedMemory.unlock();

    qDebug() << "Created new shared memory for change notifications";
    return true;
}

bool ChangeNotifier::notifyChange(ChangeType type, int recordId, const QString& extra)
{
    if (!m_sharedMemory.isAttached()) {
        qWarning() << "Shared memory not attached, cannot notify";
        return false;
    }

    ChangeNotification notification;
    notification.type = type;
    notification.recordId = recordId;
    notification.timestamp = QDateTime::currentMSecsSinceEpoch();

    // Copy extra data if provided
    if (!extra.isEmpty()) {
        QByteArray extraData = extra.toUtf8();
        int copyLen = qMin(extraData.size(), 31);
        memcpy(notification.extra, extraData.constData(), copyLen);
        notification.extra[copyLen] = '\0';
    } else {
        notification.extra[0] = '\0';
    }

    addNotification(notification);

    qDebug() << "Notified change:" << static_cast<int>(type) << "ID:" << recordId;
    return true;
}

void ChangeNotifier::startPolling(int intervalMs)
{
    if (m_pollTimer.isActive()) {
        return;
    }

    m_lastCheckTimestamp = QDateTime::currentMSecsSinceEpoch();

    connect(&m_pollTimer, &QTimer::timeout, this, &ChangeNotifier::checkForChanges);
    m_pollTimer.start(intervalMs);

    qDebug() << "Started polling for changes every" << intervalMs << "ms";
}

void ChangeNotifier::stopPolling()
{
    if (m_pollTimer.isActive()) {
        m_pollTimer.stop();
        disconnect(&m_pollTimer, &QTimer::timeout, this, &ChangeNotifier::checkForChanges);
        qDebug() << "Stopped polling for changes";
    }
}

bool ChangeNotifier::hasChange(ChangeType type, int* recordId)
{
    if (!m_sharedMemory.isAttached()) {
        return false;
    }

    m_sharedMemory.lock();

    ChangeNotification* buffer = getNotificationBuffer();
    int count = getNotificationCount();

    bool found = false;
    for (int i = 0; i < count; ++i) {
        if (buffer[i].type == type && buffer[i].timestamp > m_lastCheckTimestamp) {
            if (recordId) {
                *recordId = buffer[i].recordId;
            }
            found = true;
            break;
        }
    }

    m_sharedMemory.unlock();
    return found;
}

void ChangeNotifier::checkForChanges()
{
    if (!m_sharedMemory.isAttached()) {
        return;
    }

    m_sharedMemory.lock();

    ChangeNotification* buffer = getNotificationBuffer();
    int count = getNotificationCount();

    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();

    for (int i = 0; i < count; ++i) {
        if (buffer[i].timestamp > m_lastCheckTimestamp) {
            // Emit appropriate signal based on change type
            switch (buffer[i].type) {
            case ChangeType::PRODUCT_CHANGED:
                emit productChanged(buffer[i].recordId);
                break;
            case ChangeType::CATEGORY_CHANGED:
                emit categoryChanged(buffer[i].recordId);
                break;
            case ChangeType::TICKET_CREATED:
                emit ticketCreated(buffer[i].recordId);
                break;
            case ChangeType::TICKET_UPDATED:
                emit ticketUpdated(buffer[i].recordId);
                break;
            case ChangeType::EXPENSE_UPDATED:
                emit expenseUpdated(buffer[i].recordId);
                break;
            case ChangeType::ACCOUNT_CHANGED:
                emit accountChanged(QString::fromUtf8(buffer[i].extra));
                break;
            case ChangeType::DISCOUNT_CHANGED:
                emit discountChanged(buffer[i].recordId);
                break;
            case ChangeType::PROFILE_UPDATED:
                emit profileUpdated();
                break;
            case ChangeType::TICKET_EDIT_REQUESTED:
                emit ticketEditRequested(buffer[i].recordId);
                break;
            default:
                break;
            }
        }
    }

    m_lastCheckTimestamp = currentTime;
    m_sharedMemory.unlock();
}

ChangeNotification* ChangeNotifier::getNotificationBuffer()
{
    // First 4 bytes = count, rest = notifications
    return reinterpret_cast<ChangeNotification*>(static_cast<char*>(m_sharedMemory.data()) + sizeof(int));
}

int ChangeNotifier::getNotificationCount()
{
    return *reinterpret_cast<int*>(m_sharedMemory.data());
}

void ChangeNotifier::addNotification(const ChangeNotification& notification)
{
    m_sharedMemory.lock();

    int* countPtr = reinterpret_cast<int*>(m_sharedMemory.data());
    ChangeNotification* buffer = getNotificationBuffer();

    int currentCount = *countPtr;

    // If buffer is full, shift everything left (FIFO)
    if (currentCount >= MAX_NOTIFICATIONS) {
        memmove(&buffer[0], &buffer[1], sizeof(ChangeNotification) * (MAX_NOTIFICATIONS - 1));
        buffer[MAX_NOTIFICATIONS - 1] = notification;
    } else {
        buffer[currentCount] = notification;
        (*countPtr)++;
    }

    m_sharedMemory.unlock();
}
