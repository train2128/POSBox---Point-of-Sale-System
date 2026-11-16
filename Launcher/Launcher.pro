include(../POSBoxApp.pri)

TARGET = Launcher
TEMPLATE = app

# Sources
SOURCES += \
    src/LauncherWindow.cpp \
    src/main.cpp

HEADERS += \
    include/LauncherWindow.h
# Application icon — embedded into .exe on Windows
win32: RC_ICONS = $$PWD/../Resources/images/icons/launcher_icon.ico
