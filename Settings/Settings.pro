include(../POSBoxApp.pri)

QT += serialport

TARGET = Settings
TEMPLATE = app

# Sources
SOURCES += \
    src/Settingsmain.cpp \
    src/ReceiptDesignPage.cpp \
    src/SettingsConsoleLayout.cpp \
    src/UserDialog.cpp \
    src/settingswindow.cpp

# Headers
HEADERS += \
    include/ReceiptDesignPage.h \
    include/UserDialog.h \
    include/settingswindow.h \
    include/SettingsConstants.h

FORMS += \
    ui/settingswindow.ui

# Application icon — embedded into .exe on Windows, ignored on Linux/macOS
win32: RC_ICONS = $$PWD/../Resources/images/icons/settings_icon.ico
