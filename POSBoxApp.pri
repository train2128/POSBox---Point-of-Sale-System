# Shared project settings of every POSBox application (everything but Core).

QT += core gui widgets sql

CONFIG += c++17

include(Core/CoreIncludes.pri)

INCLUDEPATH += $$_PRO_FILE_PWD_/include \
               $$OUT_PWD

LIBS += -L$$OUT_PWD/../lib -lCore

# Find the Core shared library at runtime (Linux/macOS)
unix:!macx {
    QMAKE_LFLAGS += -Wl,-rpath,\'\$$ORIGIN/../lib\'
}

macx {
    QMAKE_LFLAGS += -Wl,-rpath,@executable_path/../lib
}

# Windows: copy Core.dll next to the executable
win32 {
    CORE_DLL = $$OUT_PWD/../lib/Core.dll

    CONFIG(debug, debug|release): CORE_OUT = $$OUT_PWD/debug
    else: CORE_OUT = $$OUT_PWD/release

    QMAKE_POST_LINK += $$quote(xcopy /Y /D \"$$shell_path($$CORE_DLL)\" \"$$shell_path($$CORE_OUT)\")
}
