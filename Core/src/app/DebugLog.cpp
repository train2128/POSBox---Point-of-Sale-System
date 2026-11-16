// POSBOX_DEBUG=1 mirrors every Qt message into a file: a Windows GUI build has
// no console, so qDebug/qWarning are otherwise invisible there. Installed from
// a static initializer so even messages emitted before main() are caught; the
// file opens lazily on the first message, by which time main() has normally
// set the application name (so the path resolves inside the app's data dir).
// stderr is mirrored too, so console runs behave exactly as before.
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QStandardPaths>
#include <QtGlobal>
#include <atomic>
#include <cstdio>
#include <cstdlib>

namespace {

void debugFileLogHandler(QtMsgType type, const QMessageLogContext&, const QString& msg)
{
    // 0 = never opened, 1 = opening (a re-entrant message must neither
    // recurse nor deadlock), 2 = ready. The open runs outside the mutex:
    // mkpath/standard-paths can themselves emit a Qt message.
    static std::atomic<int> state{0};
    static QFile file;
    static QMutex mutex;

    if (state.load(std::memory_order_acquire) == 0
        && state.exchange(1, std::memory_order_acq_rel) == 0) {
        QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        if (dir.isEmpty())
            dir = QDir::tempPath();
        QDir().mkpath(dir);
        file.setFileName(dir + QStringLiteral("/posbox_debug.log"));
        (void)file.open(QIODevice::Append | QIODevice::Text);
        if (file.isOpen() && file.size() > 5 * 1024 * 1024) {
            file.close();               // keep the file grabbable
            file.remove();
            (void)file.open(QIODevice::Append | QIODevice::Text);
        }
        state.store(2, std::memory_order_release);
    }

    if (state.load(std::memory_order_acquire) == 2) {
        QMutexLocker lock(&mutex);
        if (file.isOpen()) {
            const char* level = type == QtFatalMsg      ? "FATAL"
                                : type == QtCriticalMsg ? "CRIT"
                                : type == QtWarningMsg  ? "WARN"
                                : type == QtDebugMsg    ? "debug"
                                                        : "info";
            file.write(QDateTime::currentDateTime()
                           .toString(QStringLiteral("HH:mm:ss.zzz "))
                           .toUtf8()
                       + level + " " + msg.toUtf8() + "\n");
            file.flush();
        }
    }

    std::fprintf(stderr, "%s\n", qPrintable(msg));
    if (type == QtFatalMsg)
        std::abort();
}

struct DebugFileLogInstaller {
    DebugFileLogInstaller()
    {
        if (qEnvironmentVariableIsSet("POSBOX_DEBUG"))
            qInstallMessageHandler(debugFileLogHandler);
    }
};
const DebugFileLogInstaller s_debugFileLogInstaller;

} // namespace
