#include "ChangeNotifier.h"
#include <QDebug>
#include <QList>
#include <cstring>

namespace {

// Whether an attached segment has this build's layout. The OS may round a
// segment's reported size up (Windows reports whole memory pages), so a larger
// size within that rounding is normal. A smaller one, or one far larger, is a
// leftover from a different version of the app and would corrupt reads/writes.
bool sizeMatchesLayout(qsizetype actual, qsizetype expected)
{
    constexpr qsizetype MaxOsRounding = 64 * 1024;
    return actual >= expected && actual < expected + MaxOsRounding;
}

} // namespace

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
    const int expectedSize =
        sizeof(int) + (sizeof(ChangeNotification) * MAX_NOTIFICATIONS);

    // Already attached: only usable if the segment matches this build's
    // layout; otherwise drop it and recreate.
    if (m_sharedMemory.isAttached()) {
        if (sizeMatchesLayout(m_sharedMemory.size(), expectedSize))
            return true;
        qWarning() << "Shared memory size mismatch (have"
                   << m_sharedMemory.size() << ", expected" << expectedSize
                   << ") - detaching to recreate";
        m_sharedMemory.detach();
    }

    // Try to attach to existing shared memory
    if (m_sharedMemory.attach()) {
        if (sizeMatchesLayout(m_sharedMemory.size(), expectedSize)) {
            qDebug() << "Attached to existing shared memory";
            return true;
        }
        qWarning() << "Shared memory size mismatch (have"
                   << m_sharedMemory.size() << ", expected" << expectedSize
                   << ") - detaching to recreate";
        m_sharedMemory.detach();
    }

    // Create new shared memory
    if (!m_sharedMemory.create(expectedSize)) {
        const QString createError = m_sharedMemory.errorString();
        // Another process may have created it between our attach attempt
        // and the create: retry the attach once, and only accept the
        // segment if its size agrees with this build.
        if (m_sharedMemory.attach()) {
            if (sizeMatchesLayout(m_sharedMemory.size(), expectedSize)) {
                qDebug() << "Shared memory create raced - attached instead";
                return true;
            }
            qWarning() << "Shared memory size mismatch after create race (have"
                       << m_sharedMemory.size() << ", expected" << expectedSize
                       << ") - detaching";
            m_sharedMemory.detach();
        }
        qWarning() << "Failed to create shared memory:" << createError;
        return false;
    }

    // Initialize the buffer
    if (!m_sharedMemory.lock()) {
        qWarning() << "Failed to lock freshly created shared memory:"
                   << m_sharedMemory.errorString();
        m_sharedMemory.detach();
        return false;
    }
    memset(m_sharedMemory.data(), 0, expectedSize);
    m_sharedMemory.unlock();

    qDebug() << "Created new shared memory for change notifications";
    return true;
}

bool ChangeNotifier::notifyChange(ChangeType type, int recordId, const QString& extra)
{
    if (!m_sharedMemory.isAttached()) {
        // Self-heal: the constructor's initialize() can fail (e.g. a stale
        // segment held by an older build); by now that process may be gone.
        if (!initialize()) {
            qWarning() << "Shared memory not attached, cannot notify";
            return false;
        }
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

    if (!addNotification(notification))
        return false;

    qDebug() << "Notified change:" << static_cast<int>(type) << "ID:" << recordId;
    return true;
}

void ChangeNotifier::startPolling(int intervalMs)
{
    if (m_pollTimer.isActive()) {
        return;
    }

    m_lastCheckTimestamp = QDateTime::currentMSecsSinceEpoch() - 1;

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

void ChangeNotifier::checkForChanges()
{
    if (!m_sharedMemory.isAttached()) {
        return;
    }

    if (!m_sharedMemory.lock()) {
        qWarning() << "Failed to lock shared memory:" << m_sharedMemory.errorString();
        return;
    }

    ChangeNotification* buffer = getNotificationBuffer();
    const int count = getNotificationCount();

    QList<ChangeNotification> fresh;
    for (int i = 0; i < count; ++i) {
        if (buffer[i].timestamp > m_lastCheckTimestamp) {
            // Advance to the newest timestamp delivered, NOT to "now": a
            // notification written after this read but within the same
            // millisecond must still be greater than this on the next poll.
            m_lastCheckTimestamp = qMax(m_lastCheckTimestamp, buffer[i].timestamp);
            fresh.append(buffer[i]);
        }
    }

    m_sharedMemory.unlock();

    // Delivered after unlocking: receivers may run slow queries, and holding
    // this cross-process lock during them would stall every other POSBox app.
    for (const ChangeNotification& notification : fresh)
        deliver(notification);
}

void ChangeNotifier::deliver(const ChangeNotification& notification)
{
    switch (notification.type) {
    case ChangeType::PRODUCT_CHANGED:
        emit productChanged(notification.recordId);
        break;
    case ChangeType::CATEGORY_CHANGED:
        emit categoryChanged(notification.recordId);
        break;
    case ChangeType::TICKET_CREATED:
        emit ticketCreated(notification.recordId);
        break;
    case ChangeType::TICKET_UPDATED:
        emit ticketUpdated(notification.recordId);
        break;
    case ChangeType::EXPENSE_UPDATED:
        emit expenseUpdated(notification.recordId);
        break;
    case ChangeType::DISCOUNT_CHANGED:
        emit discountChanged(notification.recordId);
        break;
    case ChangeType::SESSION_LOGOUT_REQUESTED:
        emit sessionLogoutRequested();
        break;
    case ChangeType::APP_OPEN_REQUESTED:
        emit appOpenRequested(QString::fromUtf8(notification.extra));
        break;
    case ChangeType::APP_NAVIGATE_REQUESTED:
        emit appNavigateRequested(QString::fromUtf8(notification.extra));
        break;
    default:
        break;
    }
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

bool ChangeNotifier::addNotification(const ChangeNotification& notification)
{
    if (!m_sharedMemory.isAttached()) {
        qWarning() << "Shared memory not attached, cannot add notification";
        return false;
    }

    if (!m_sharedMemory.lock()) {
        qWarning() << "Failed to lock shared memory:" << m_sharedMemory.errorString();
        return false;
    }

    int* countPtr = reinterpret_cast<int*>(m_sharedMemory.data());
    ChangeNotification* buffer = getNotificationBuffer();

    int currentCount = *countPtr;

    // Timestamps must be strictly increasing: readers deliver "newer than
    // the last one they saw", so two notifications written in the same
    // millisecond must not share a timestamp (the second would be dropped).
    ChangeNotification n = notification;
    if (currentCount > 0) {
        const int lastIdx = (currentCount >= MAX_NOTIFICATIONS)
                                ? MAX_NOTIFICATIONS - 1
                                : currentCount - 1;
        if (n.timestamp <= buffer[lastIdx].timestamp)
            n.timestamp = buffer[lastIdx].timestamp + 1;
    }

    // If buffer is full, shift everything left (FIFO)
    if (currentCount >= MAX_NOTIFICATIONS) {
        memmove(&buffer[0], &buffer[1], sizeof(ChangeNotification) * (MAX_NOTIFICATIONS - 1));
        buffer[MAX_NOTIFICATIONS - 1] = n;
    } else {
        buffer[currentCount] = n;
        (*countPtr)++;
    }

    m_sharedMemory.unlock();
    return true;
}
