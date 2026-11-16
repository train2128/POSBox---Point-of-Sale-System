#include "LocalImages.h"
#include "LocalApi.h"
#include <QBuffer>
#include <QFileInfo>
#include <QImageReader>
#include <QMutexLocker>
#include <QThread>

// Loads and scales pictures off the GUI thread.
class LocalImagesWorker : public QObject
{
    Q_OBJECT
public slots:
    void process(const QString& filename, int maxSize)
    {
        emit imageReady(filename, maxSize, load(filename, maxSize));
    }

signals:
    void imageReady(const QString& filename, int maxSize, const QImage& image);

public:
    static QImage load(const QString& filename, int maxSize)
    {
        const QString path = LocalApi::imagePath(filename);
        if (path.isEmpty())
            return {};
        QImageReader reader(path);
        reader.setAutoTransform(true);  // photos taken sideways stand upright
        QImage image = reader.read();
        if (image.isNull())
            return {};
        if (maxSize > 0 && (image.width() > maxSize || image.height() > maxSize))
            image = image.scaled(maxSize, maxSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        return image;
    }
};

LocalImages::LocalImages(LocalApi* /*api*/, QObject* parent)
    : QObject(parent)
{
    qRegisterMetaType<QImage>("QImage");
    m_thread = new QThread(this);
    m_worker = new LocalImagesWorker;
    m_worker->moveToThread(m_thread);
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(this, &LocalImages::requestImage, m_worker, &LocalImagesWorker::process);
    connect(m_worker, &LocalImagesWorker::imageReady, this, &LocalImages::onWorkerImage);
    m_thread->start();
}

LocalImages::~LocalImages()
{
    m_thread->quit();
    m_thread->wait();
}

QString LocalImages::cacheKey(const QString& name, int maxSize)
{
    return name + QLatin1Char('@') + QString::number(maxSize);
}

void LocalImages::requestPixmap(const QString& filename, int maxSize)
{
    if (filename.isEmpty())
        return;
    {
        QMutexLocker lock(&m_mutex);
        const auto it = m_cache.constFind(cacheKey(filename, maxSize));
        if (it != m_cache.constEnd()) {
            const QImage image = it.value().toImage();
            lock.unlock();
            emit pixmapReady(filename, maxSize, image);
            return;
        }
    }
    emit requestImage(filename, maxSize);
}

void LocalImages::onWorkerImage(const QString& filename, int maxSize, const QImage& image)
{
    if (!image.isNull()) {
        QMutexLocker lock(&m_mutex);
        m_cache.insert(cacheKey(filename, maxSize), QPixmap::fromImage(image));
    }
    emit pixmapReady(filename, maxSize, image);
}

QImage LocalImages::image(const QString& filename)
{
    return pixmap(filename).toImage();
}

QPixmap LocalImages::pixmap(const QString& filename)
{
    if (filename.isEmpty())
        return {};
    {
        QMutexLocker lock(&m_mutex);
        const auto it = m_cache.constFind(cacheKey(filename, 0));
        if (it != m_cache.constEnd())
            return it.value();
    }
    const QImage image = LocalImagesWorker::load(filename, 0);
    if (image.isNull())
        return {};
    const QPixmap pix = QPixmap::fromImage(image);
    QMutexLocker lock(&m_mutex);
    m_cache.insert(cacheKey(filename, 0), pix);
    return pix;
}

bool LocalImages::isCached(const QString& filename, int maxSize) const
{
    QMutexLocker lock(&m_mutex);
    return m_cache.contains(cacheKey(filename, maxSize));
}

QPixmap LocalImages::thumbnail(const QString& filename, int maxSize) const
{
    QMutexLocker lock(&m_mutex);
    return m_cache.value(cacheKey(filename, maxSize));
}

void LocalImages::clear()
{
    QMutexLocker lock(&m_mutex);
    m_cache.clear();
}

void LocalImages::remember(const QString& filename, const QByteArray& data)
{
    QImage image;
    image.loadFromData(data);
    QMutexLocker lock(&m_mutex);
    // Drop every cached size of the old picture, then keep the new one.
    const QString prefix = filename + QLatin1Char('@');
    for (auto it = m_cache.begin(); it != m_cache.end();) {
        if (it.key().startsWith(prefix))
            it = m_cache.erase(it);
        else
            ++it;
    }
    if (!image.isNull())
        m_cache.insert(cacheKey(filename, 0), QPixmap::fromImage(image));
}

#include "LocalImages.moc"
