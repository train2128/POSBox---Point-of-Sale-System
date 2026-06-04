QT += core gui widgets sql

CONFIG += c++17

TARGET = Register
TEMPLATE = app

# Include Core library headers
INCLUDEPATH += $$PWD/../Core/include \
               $$PWD/include \
               $$OUT_PWD

# Link against Core library
LIBS += -L$$OUT_PWD/../lib -lCore

# Sources
SOURCES += \
    src/CartItemWidget.cpp \
    src/CustomItemDialog.cpp \
    src/DiscountDialog.cpp \
    src/DiscountHandler.cpp \
    src/DiscountItemWidget.cpp \
    src/OpenTicketsDialog.cpp \
    src/ProductButton.cpp \
    src/ProductButtonCreator.cpp \
    src/RegisterWindow.cpp \
    src/SaveTicketDialog.cpp \
    src/UIMap.cpp \
    src/VariantPickerDialog.cpp \
    src/main.cpp

# Headers
HEADERS += \
    include/CartItemWidget.h \
    include/CustomItemDialog.h \
    include/DiscountDialog.h \
    include/DiscountHandler.h \
    include/DiscountItemWidget.h \
    include/OpenTicketsDialog.h \
    include/ProductButton.h \
    include/ProductButtonCreator.h \
    include/RegisterConstants.h \
    include/RegisterWindow.h \
    include/SaveTicketDialog.h \
    include/UIMap.h \
    include/VariantPickerDialog.h

FORMS += \
    $$PWD/ui/RegisterWindow.ui

# Resources
RESOURCES += \
    $$PWD/../Resources/Resources.qrc

# Application icon — embedded into .exe on Windows, ignored on Linux/macOS
win32: RC_ICONS = $$PWD/../Resources/images/icons/register_icon.ico

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
