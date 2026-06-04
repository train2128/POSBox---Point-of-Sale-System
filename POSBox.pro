TEMPLATE = subdirs

# Build order
SUBDIRS += \
    Core \
    Register \
    Inventory \
    Tickets \
    Settings \
    Financial

# Dependencies - apps depend on Core
Register.depends = Core
Inventory.depends = Core
Tickets.depends = Core
Financial.depends = Core
Settings.depends = Core
