QT += core widgets serialport sql

TEMPLATE = lib
CONFIG += shared c++17

TARGET = Core

DEFINES += CORE_LIBRARY

# MinGW stops exporting symbols automatically once one is exported explicitly
# (CORE_EXPORT). Unmarked Core code must stay visible to the applications.
win32-g++: QMAKE_LFLAGS += -Wl,--export-all-symbols
# PlaySound for the notification chime
win32: LIBS += -lwinmm

# Output directory
DESTDIR = $$OUT_PWD/../lib

include(CoreIncludes.pri)

# Internal headers of the data layer
INCLUDEPATH += $$PWD/src/data

HEADERS += \
    include/data/DatabaseCreator.h \
    include/data/LocalApi.h \
    include/data/LocalDb.h \
    include/data/LocalImages.h \
    include/i18n/CountryData.h \
    include/i18n/TranslationManager.h \
    src/data/NotificationsService.h \
    src/data/QueryService.h \
    src/data/SalesService.h \
    include/app/AppLock.h \
    include/app/AppRuntime.h \
    include/app/CommonConfig.h \
    include/app/CoreExport.h \
    include/app/LocalSettings.h \
    include/app/Prefixes.h \
    include/notifications/ChangeNotifier.h \
    include/notifications/NotificationActions.h \
    include/notifications/NotificationBadge.h \
    include/notifications/NotificationBell.h \
    include/notifications/NotificationManager.h \
    include/notifications/NotificationPanel.h \
    include/notifications/NotificationSound.h \
    include/printing/PrinterConstants.h \
    include/printing/ReceiptDesign.h \
    include/printing/ThermalPrinter.h \
    include/session/AccessControl.h \
    include/session/ActiveSessionManager.h \
    include/session/LogoutManager.h \
    include/session/Permissions.h \
    include/session/SessionManager.h \
    include/session/SessionResolver.h \
    include/session/UserSession.h \
    include/ui/Animations.h \
    include/ui/ComboStyle.h \
    include/ui/LineIcons.h \
    include/ui/MessageDialog.h \
    include/ui/ModernDialog.h \
    include/ui/StyleManager.h \
    include/ui/ElidedLabel.h \
    include/ui/TableDelegates.h \
    include/ui/ToggleSwitch.h

SOURCES += \
    src/data/DatabaseCreator.cpp \
    src/data/LocalApi.cpp \
    src/data/LocalDb.cpp \
    src/data/LocalImages.cpp \
    src/data/NotificationsService.cpp \
    src/data/QueryService.cpp \
    src/data/SalesService.cpp \
    src/i18n/CountryData.cpp \
    src/i18n/TranslationManager.cpp \
    src/app/AppLock.cpp \
    src/app/AppRuntime.cpp \
    src/app/CommonConfig.cpp \
    src/app/DebugLog.cpp \
    src/app/LocalSettings.cpp \
    src/app/Prefixes.cpp \
    src/notifications/ChangeNotifier.cpp \
    src/notifications/NotificationBell.cpp \
    src/notifications/NotificationManager.cpp \
    src/notifications/NotificationPanel.cpp \
    src/notifications/NotificationSound.cpp \
    src/printing/ReceiptDesign.cpp \
    src/printing/ThermalPrinter.cpp \
    src/session/AccessControl.cpp \
    src/session/ActiveSessionManager.cpp \
    src/session/LogoutManager.cpp \
    src/session/Permissions.cpp \
    src/session/SessionManager.cpp \
    src/session/SessionResolver.cpp \
    src/session/UserSession.cpp \
    src/ui/Animations.cpp \
    src/ui/ComboStyle.cpp \
    src/ui/LineIcons.cpp \
    src/ui/MessageDialog.cpp \
    src/ui/ModernDialog.cpp \
    src/ui/StyleManager.cpp \
    src/ui/TableDelegates.cpp \
    src/ui/ToggleSwitch.cpp

RESOURCES += \
    src/data/DatabaseCreator.cpp \
    src/data/LocalApi.cpp \
    src/data/LocalDb.cpp \
    src/data/LocalImages.cpp \
    src/data/NotificationsService.cpp \
    src/data/QueryService.cpp \
    src/data/SalesService.cpp \
    src/i18n/CountryData.cpp \
    src/i18n/TranslationManager.cpp \
    $$PWD/../Resources/Resources.qrc

unix {
    target.path = /usr/lib
    INSTALLS += target
}
