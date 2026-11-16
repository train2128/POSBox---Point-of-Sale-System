include(../POSBoxApp.pri)

QT += printsupport

TARGET = Tickets
TEMPLATE = app

# Sources
SOURCES += \
    src/InvoicePainter.cpp \
    src/TicketPrintManager.cpp \
    src/TicketWindow.cpp \
    src/TicketsMain.cpp
# Headers
HEADERS += \
    include/InvoicePainter.h \
    include/TicketPrintManager.h \
    include/TicketWindow.h
FORMS += \
    ui/TicketWindow.ui

# Application icon — embedded into .exe on Windows, ignored on Linux/macOS
win32: RC_ICONS = $$PWD/../Resources/images/icons/tickets_icon.ico
