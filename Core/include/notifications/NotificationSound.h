#ifndef NOTIFICATIONSOUND_H
#define NOTIFICATIONSOUND_H

#include "CoreExport.h"

// The chime played when a new notification appears (POSBox 2.1.5-c).
//
// Every open POSBox app receives the same announcement, so the apps on one PC
// agree through shared memory that only the first one plays it. Uses what the
// system already has (Windows: PlaySound; Linux: pw-play / paplay / aplay),
// so no extra Qt module is needed. Off when "Notification sound" is off in
// App Settings.
namespace NotificationSound {

// Plays the chime unless it is turned off or another app on this PC just
// played it.
CORE_EXPORT void play();

// Plays it regardless of the setting and the other apps (the Settings
// preview button).
CORE_EXPORT void preview();

} // namespace NotificationSound

#endif // NOTIFICATIONSOUND_H
