QT += core gui widgets sql

CONFIG += c++17

TARGET = Inventory
TEMPLATE = app

# Include Core library headers
INCLUDEPATH += $$PWD/../Core/include \
               $$PWD/include \
               $$OUT_PWD

# Link against Core library
LIBS += -L$$OUT_PWD/../lib -lCore

# Sources
SOURCES += \
    src/InventoryUIMap.cpp \
    src/InventoryWindow.cpp \
    src/ProductDialog.cpp \
    src/VariantsDialog.cpp \
    src/main.cpp

# Headers
HEADERS += \
    include/InventoryConstants.h \
    include/InventoryUIMap.h \
    include/InventoryWindow.h \
    include/ProductDialog.h \
    include/VariantsDialog.h

# Forms
FORMS += \
    ui/Invenorywindow.ui \
    ui/productdialog.ui \
    ui/Variantsdialog.ui

# Resources
RESOURCES += \
    $$PWD/../Resources/Resources.qrc

# Application icon — embedded into .exe on Windows, ignored on Linux/macOS
win32: RC_ICONS = $$PWD/../Resources/images/icons/inventory_icon.ico

# Set RPATH for finding shared library at runtime (Linux/macOS)
unix:!macx {
    QMAKE_LFLAGS += -Wl,-rpath,\'\$$ORIGIN/../lib\'
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
