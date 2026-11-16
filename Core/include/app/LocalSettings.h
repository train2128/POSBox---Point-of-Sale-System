#ifndef LOCALSETTINGS_H
#define LOCALSETTINGS_H

#include <QString>
#include <QVariantMap>

// Per-machine preferences (theme, background, register display, printer).
// Commercial Version: stored in the local database (app_settings and
// printer_settings), so a database backup carries them too.
class LocalSettings
{
public:
    QString colorScheme = "dark";   // dark | light
    QString fontSize = "medium";    // small | medium | large
    QString backgroundColor;        // hex #RRGGBB tint for the light bg (empty = default caramel)
    QString backgroundImage;        // absolute path of a copied bg image (empty = none)
    QString backgroundScale = "cover"; // cover | stretch | fit (how the bg image fills)
    int backgroundDim = 0;          // 0-100 % black scrim over the bg image (legibility)
    bool showOosProducts = true;
    bool animationsEnabled = true;
    bool notificationSound = true;  // chime when a new notification appears
    QVariantMap printer;  // enabled, port, baud_rate, data_bits, parity,
                          // stop_bits, flow_control, latency_ms, paper_size, encoding

    // Cheap to call repeatedly: the values are cached for a moment (another
    // POSBox app may change them) and refreshed after save().
    static LocalSettings load();
    bool save() const;
    static QVariantMap defaultPrinter();
};

#endif // LOCALSETTINGS_H
