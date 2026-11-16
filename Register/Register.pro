include(../POSBoxApp.pri)

TARGET = Register
TEMPLATE = app

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
    src/RegisterLayout.cpp \
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

# Application icon — embedded into .exe on Windows, ignored on Linux/macOS
win32: RC_ICONS = $$PWD/../Resources/images/icons/register_icon.ico
