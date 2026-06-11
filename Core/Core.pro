QT += core sql widgets serialport network

TEMPLATE = lib
CONFIG += shared c++17
DEFINES += CORE_LIBRARY

TARGET = Core

# Output directory
DESTDIR = $$OUT_PWD/../lib

# Include paths
INCLUDEPATH += $$PWD/include \

# Headers
HEADERS += \
    include/DatabaseCreator.h \
    include/DatabaseManager.h \
    include/Prefixes.h \
    include/QueryRepository.h \
    include/ChangeNotifier.h \
    include/CommonConfig.h \
    include/CountryData.h \
    include/IAuthenticationProvider.h \
    include/LocalDatabaseAuth.h \
    include/RegisterLock.h \
    include/TicketEditRequest.h \
    include/TicketsLock.h \
    include/UserSession.h \
    include/SessionManager.h \
    include/AccessControl.h \
    include/PermissionManager.h \
    include/StyleManager.h \
    include/LoginDialog.h \
    include/PrinterConstants.h \
    include/ThermalPrinter.h \
    include/NotificationManager.h \
    include/NotificationSettings.h \
    include/NotificationPanel.h \
    include/ActiveSessionManager.h \
    include/LogoutManager.h \
    include/TranslationManager.h

# Sources
SOURCES += \
    src/DatabaseCreator.cpp \
    src/DatabaseManager.cpp \
    src/Prefixes.cpp \
    src/QueryRepository.cpp \
    src/ChangeNotifier.cpp \
    src/CommonConfig.cpp \
    src/CountryData.cpp \
    src/LocalDatabaseAuth.cpp \
    src/UserSession.cpp \
    src/SessionManager.cpp \
    src/AccessControl.cpp \
    src/PermissionManager.cpp \
    src/StyleManager.cpp \
    src/LoginDialog.cpp \
    src/ThermalPrinter.cpp \
    src/NotificationManager.cpp \
    src/NotificationSettings.cpp \
    src/NotificationPanel.cpp \
    src/ActiveSessionManager.cpp \
    src/LogoutManager.cpp \
    src/TranslationManager.cpp

# Forms
FORMS += \
    ui/LoginDialog.ui

RESOURCES += \
    $$PWD/../Resources/Resources.qrc

# Install rules (optional)
unix {
    target.path = /usr/lib
    INSTALLS += target
}
