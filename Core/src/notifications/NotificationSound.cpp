#include "NotificationSound.h"
#include "CommonConfig.h"
#include "LocalSettings.h"

#include <QByteArray>
#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSaveFile>
#include <QSharedMemory>
#include <QStandardPaths>
#include <QStringList>

#ifdef Q_OS_WIN
#include <windows.h>
#include <mmsystem.h>
#endif

namespace {

constexpr const char* SoundResource = ":/sounds/sounds/notification.wav";
constexpr const char* SharedKey = "POSBoxNotificationSound";
// Apps get the same push within milliseconds; a second announcement this
// soon after the last chime is the same moment and stays quiet.
constexpr qint64 QuietWindowMs = 3000;

const QByteArray& soundBytes()
{
    static const QByteArray bytes = []() {
        QFile f(QString::fromLatin1(SoundResource));
        return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
    }();
    return bytes;
}

// True for exactly one app on this PC per moment.
bool claimChime()
{
    static QSharedMemory shared(QString::fromLatin1(SharedKey));
    if (!shared.isAttached() && !shared.attach()) {
        if (!shared.create(sizeof(qint64)) && !shared.attach())
            return true;   // can't coordinate: better a double chime than none
    }
    if (shared.size() < static_cast<qsizetype>(sizeof(qint64)) || !shared.lock())
        return true;

    auto* last = static_cast<qint64*>(shared.data());
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const bool mine = (now - *last) >= QuietWindowMs || now < *last;
    if (mine)
        *last = now;
    shared.unlock();
    return mine;
}

#ifndef Q_OS_WIN
// The players read a file: keep a copy of the chime in the app data folder.
QString soundFile()
{
    const QByteArray& bytes = soundBytes();
    const QString path = POSConfig::getAppDataPath() + "notification.wav";
    QFile existing(path);
    if (existing.size() == bytes.size())
        return path;
    QSaveFile out(path);
    if (out.open(QIODevice::WriteOnly) && out.write(bytes) == bytes.size() && out.commit())
        return path;
    return QString();
}
#endif

void playNow()
{
    if (soundBytes().isEmpty()) {
        qWarning() << "Notification sound missing from resources";
        return;
    }
#ifdef Q_OS_WIN
    // Async from memory; the bytes are static, so they outlive the playback.
    PlaySoundA(soundBytes().constData(), nullptr, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
#else
    const QString file = soundFile();
    if (file.isEmpty())
        return;

    struct Player { const char* exe; QStringList args; };
    const Player players[] = {
#ifdef Q_OS_MACOS
        {"afplay",  {}},
#endif
        {"pw-play", {}},
        {"paplay",  {}},
        {"aplay",   {QStringLiteral("-q")}},
    };
    for (const Player& p : players) {
        const QString exe = QStandardPaths::findExecutable(QString::fromLatin1(p.exe));
        if (exe.isEmpty())
            continue;
        QProcess proc;
        proc.setProgram(exe);
        proc.setArguments(QStringList(p.args) << file);
        // A system tool: don't hand it the bundled Qt libraries (AppImage).
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.remove("LD_LIBRARY_PATH");
        env.remove("QT_PLUGIN_PATH");
        proc.setProcessEnvironment(env);
        proc.setStandardOutputFile(QProcess::nullDevice());
        proc.setStandardErrorFile(QProcess::nullDevice());
        if (proc.startDetached())
            return;
    }
    qWarning() << "Notification sound: no audio player found (pw-play, paplay or aplay)";
#endif
}

} // namespace

namespace NotificationSound {

void play()
{
    if (!LocalSettings::load().notificationSound)
        return;
    if (!claimChime())
        return;
    playNow();
}

void preview()
{
    playNow();
}

} // namespace NotificationSound
