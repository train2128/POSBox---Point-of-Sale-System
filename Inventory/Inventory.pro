include(../POSBoxApp.pri)

TARGET = Inventory
TEMPLATE = app

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
    ui/Invenorywindow.ui

# Application icon — embedded into .exe on Windows, ignored on Linux/macOS
win32: RC_ICONS = $$PWD/../Resources/images/icons/inventory_icon.ico
