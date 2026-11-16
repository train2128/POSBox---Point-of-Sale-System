#include "LocalSettings.h"
#include "CommonConfig.h"
#include <QDateTime>
#include <QDebug>
#include <QMutex>
#include <QMutexLocker>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QThread>

namespace {

// Settings are read often (themes, animations on every hover), so a parsed
// copy is reused for a short while. Another app's change shows up after it.
constexpr qint64 CacheMs = 1500;

struct SettingsCache {
    QMutex mutex;
    bool valid = false;
    qint64 loadedAt = 0;
    LocalSettings settings;
};

SettingsCache& settingsCache()
{
    static SettingsCache cache;
    return cache;
}

// A connection of its own, so settings can be read before (and independently
// of) the app's main database connection.
QSqlDatabase settingsDb()
{
    const QString name = QStringLiteral("posbox_localsettings_%1")
                             .arg(quintptr(QThread::currentThreadId()));
    if (QSqlDatabase::contains(name)) {
        QSqlDatabase db = QSqlDatabase::database(name, false);
        if (!db.isOpen())
            db.open();
        return db;
    }
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", name);
    db.setDatabaseName(POSConfig::getDatabasePath());
    db.setConnectOptions("QSQLITE_BUSY_TIMEOUT=3000");
    if (!db.open())
        qWarning() << "LocalSettings: cannot open database:" << db.lastError().text();
    return db;
}

bool hasColumn(const QSqlRecord& rec, const char* name)
{
    return rec.indexOf(QLatin1String(name)) >= 0;
}

LocalSettings readSettings()
{
    LocalSettings s;
    s.printer = LocalSettings::defaultPrinter();
    QSqlDatabase db = settingsDb();
    if (!db.isOpen())
        return s;

    QSqlQuery q(db);
    if (q.exec("SELECT * FROM app_settings WHERE id = 1") && q.next()) {
        const QSqlRecord rec = q.record();
        auto text = [&](const char* col, const QString& fallback) {
            if (!hasColumn(rec, col) || q.value(col).isNull())
                return fallback;
            const QString v = q.value(col).toString();
            return v.isEmpty() ? fallback : v;
        };
        auto flag = [&](const char* col, bool fallback) {
            return (hasColumn(rec, col) && !q.value(col).isNull()) ? q.value(col).toInt() != 0 : fallback;
        };
        s.colorScheme = text("color_scheme", s.colorScheme);
        s.fontSize = text("font_size", s.fontSize);
        s.backgroundColor = text("background_color", QString());
        s.backgroundImage = text("background_image", QString());
        s.backgroundScale = text("background_scale", s.backgroundScale);
        if (s.backgroundScale != "stretch" && s.backgroundScale != "fit")
            s.backgroundScale = "cover";
        s.backgroundDim = hasColumn(rec, "background_dim")
                              ? qBound(0, q.value("background_dim").toInt(), 100) : 0;
        s.showOosProducts = flag("show_oos_products", true);
        s.animationsEnabled = flag("animations_enabled", true);
        s.notificationSound = flag("notification_sound", true);
    }

    QSqlQuery p(db);
    if (p.exec("SELECT * FROM printer_settings WHERE id = 1") && p.next()) {
        const QSqlRecord rec = p.record();
        for (int i = 0; i < rec.count(); ++i) {
            const QString col = rec.fieldName(i);
            if (col == "id" || p.value(i).isNull())
                continue;
            s.printer[col] = p.value(i);
        }
    }
    return s;
}

} // namespace

QVariantMap LocalSettings::defaultPrinter()
{
    return QVariantMap{
        {"enabled", 0}, {"port", QString()}, {"baud_rate", 9600},
        {"data_bits", 8}, {"parity", "None"}, {"stop_bits", "1"},
        {"flow_control", "None"}, {"latency_ms", 50},
        {"paper_size", "80mm"}, {"encoding", "UTF-8"},
    };
}

LocalSettings LocalSettings::load()
{
    SettingsCache& cache = settingsCache();
    QMutexLocker lock(&cache.mutex);
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (!cache.valid || now - cache.loadedAt > CacheMs) {
        cache.settings = readSettings();
        cache.loadedAt = now;
        cache.valid = true;
    }
    return cache.settings;
}

bool LocalSettings::save() const
{
    QSqlDatabase db = settingsDb();
    if (!db.isOpen())
        return false;

    bool ok = true;
    QSqlQuery seed(db);
    seed.exec("INSERT OR IGNORE INTO app_settings (id) VALUES (1)");
    seed.exec("INSERT OR IGNORE INTO printer_settings (id) VALUES (1)");

    QSqlQuery q(db);
    q.prepare("UPDATE app_settings SET color_scheme = ?, font_size = ?, background_color = ?, "
              "background_image = ?, background_scale = ?, background_dim = ?, "
              "show_oos_products = ?, animations_enabled = ?, notification_sound = ? WHERE id = 1");
    q.addBindValue(colorScheme);
    q.addBindValue(fontSize);
    q.addBindValue(backgroundColor);
    q.addBindValue(backgroundImage);
    q.addBindValue(backgroundScale);
    q.addBindValue(qBound(0, backgroundDim, 100));
    q.addBindValue(showOosProducts ? 1 : 0);
    q.addBindValue(animationsEnabled ? 1 : 0);
    q.addBindValue(notificationSound ? 1 : 0);
    if (!q.exec()) {
        qWarning() << "LocalSettings: saving app settings failed:" << q.lastError().text();
        ok = false;
    }

    static const QStringList printerColumns = {
        "enabled", "port", "baud_rate", "data_bits", "parity", "stop_bits",
        "flow_control", "latency_ms", "paper_size", "encoding"};
    QStringList sets;
    QVariantList values;
    for (const QString& col : printerColumns) {
        if (!printer.contains(col))
            continue;
        sets << col + " = ?";
        values << printer.value(col);
    }
    if (!sets.isEmpty()) {
        QSqlQuery p(db);
        p.prepare("UPDATE printer_settings SET " + sets.join(", ") + " WHERE id = 1");
        for (const QVariant& v : values)
            p.addBindValue(v);
        if (!p.exec()) {
            qWarning() << "LocalSettings: saving printer settings failed:" << p.lastError().text();
            ok = false;
        }
    }

    QMutexLocker lock(&settingsCache().mutex);
    settingsCache().valid = false;
    return ok;
}
