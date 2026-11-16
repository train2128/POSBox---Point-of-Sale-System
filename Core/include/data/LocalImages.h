#ifndef LOCALIMAGES_H
#define LOCALIMAGES_H

#include "CoreExport.h"
#include <QHash>
#include <QImage>
#include <QMutex>
#include <QObject>
#include <QPixmap>
#include <QString>

class LocalApi;
class QThread;
class LocalImagesWorker;

// Product/logo pictures (Commercial Version: files in the app-data folder).
// Loading, decoding and scaling run on a worker thread so the product grid
// never waits; scaled copies are kept in RAM per requested size.
class CORE_EXPORT LocalImages : public QObject
{
    Q_OBJECT

public:
    explicit LocalImages(LocalApi* api = nullptr, QObject* parent = nullptr);
    ~LocalImages();

    // Async request: the worker loads the file and emits pixmapReady().
    void requestPixmap(const QString& filename, int maxSize = 0);

    // Synchronous convenience for one-off images (logos). Null when missing.
    QPixmap pixmap(const QString& filename);
    QImage image(const QString& filename);

    // RAM cache checks (no disk I/O).
    bool isCached(const QString& filename, int maxSize = 0) const;
    QPixmap thumbnail(const QString& filename, int maxSize) const;

    void clear();

    // A picture this app just saved: shown at once, even if a previous version
    // with the same name is cached.
    void remember(const QString& filename, const QByteArray& data);

signals:
    // Emitted on the GUI thread when a requested image is ready.
    // `image` is null when the file is missing or unreadable.
    void pixmapReady(const QString& filename, int maxSize, const QImage& image);

    // Internal: GUI thread -> worker thread.
    void requestImage(const QString& filename, int maxSize);

private slots:
    void onWorkerImage(const QString& filename, int maxSize, const QImage& image);

private:
    static QString cacheKey(const QString& name, int maxSize);

    LocalImagesWorker* m_worker = nullptr;
    QThread* m_thread = nullptr;

    mutable QMutex m_mutex;
    QHash<QString, QPixmap> m_cache;  // key: "name@size" (0 = full size)
};

#endif // LOCALIMAGES_H
