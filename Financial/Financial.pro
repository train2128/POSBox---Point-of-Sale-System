include(../POSBoxApp.pri)

QT += printsupport

TARGET = Financial
TEMPLATE = app

# Sources
SOURCES += \
    src/FinancialUIMap.cpp \
    src/FinancialLayout.cpp \
    src/FinancialWidgets.cpp \
    src/FinancialWindow.cpp \
    src/main.cpp

# Headers
HEADERS += \
    include/FinancialConstants.h \
    include/FinancialUIMap.h \
    include/FinancialWidgets.h \
    include/FinancialWindow.h
FORMS += \
    $$PWD/ui/financialwindow.ui

# Application icon — embedded into .exe on Windows, ignored on Linux/macOS
win32: RC_ICONS = $$PWD/../Resources/images/icons/financial_icon.ico
