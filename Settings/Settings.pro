QT += core gui widgets serialport sql

CONFIG += c++17

TARGET = Settings
TEMPLATE = app

# Include Core library headers
INCLUDEPATH += $$PWD/../Core/include \
               $$PWD/include \
               $$OUT_PWD

# Link against Core library
LIBS += -L$$OUT_PWD/../lib -lCore

# Sources
SOURCES += \
    src/Settingsmain.cpp \
    src/UserDialog.cpp \
    src/settingswindow.cpp

# Headers
HEADERS += \
    include/UserDialog.h \
    include/settingswindow.h \
    include/SettingsConstants.h

FORMS += \
    ui/settingswindow.ui

# Resources
RESOURCES += \
    $$PWD/../Resources/Resources.qrc

# Application icon — embedded into .exe on Windows, ignored on Linux/macOS
win32: RC_ICONS = $$PWD/../Resources/images/icons/settings_icon.ico

# Set RPATH for finding shared library at runtime (Linux/macOS)
unix:!macx {
    QMAKE_LFLAGS += -Wl,-rpath,\'$$ORIGIN/../lib\'
}

macx {
    QMAKE_LFLAGS += -Wl,-rpath,@executable_path/../lib
}

# Windows: Copy Core.dll to output directory
win32 {
    CONFIG(debug, debug|release) {
        CORE_DLL = $$OUT_PWD/../lib/Cored.dll
    } else {
        CORE_DLL = $$OUT_PWD/../lib/Core.dll
    }

    QMAKE_POST_LINK += $$quote(xcopy /Y /D \"$$shell_path($$CORE_DLL)\" \"$$shell_path($$OUT_PWD)\")
}
