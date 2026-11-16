// The Settings window's layout: a sidebar menu with one topic per page.
// It only arranges the controls created in settingswindow.ui, so all their
// connections and save/load code in settingswindow.cpp stay as they are.
#include "settingswindow.h"
#include "ui_settingswindow.h"
#include "Animations.h"
#include "CommonConfig.h"
#include "LineIcons.h"
#include "ReceiptDesignPage.h"
#include "TableDelegates.h"
#include <QButtonGroup>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QScrollArea>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>

namespace {

QLabel* styled(QLabel* label, const char* role)
{
    label->setProperty("uiRole", role);
    return label;
}

QLabel* text(const QString& value, const char* role)
{
    auto* label = styled(new QLabel(value), role);
    return label;
}

QFrame* separator()
{
    auto* line = new QFrame;
    line->setProperty("uiSep", true);
    return line;
}

// A rounded card with an optional heading; `body` receives its content layout.
QFrame* card(QVBoxLayout** body, const QString& title = QString(), const QString& subtitle = QString())
{
    auto* frame = new QFrame;
    frame->setProperty("uiCard", true);
    auto* layout = new QVBoxLayout(frame);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(12);
    if (!title.isEmpty()) {
        auto* heading = new QVBoxLayout;
        heading->setSpacing(1);
        heading->addWidget(text(title, "cardTitle"));
        if (!subtitle.isEmpty()) {
            QLabel* sub = text(subtitle, "desc");
            sub->setWordWrap(true);
            heading->addWidget(sub);
        }
        layout->addLayout(heading);
    }
    *body = layout;
    return frame;
}

QWidget* inRow(std::initializer_list<QWidget*> widgets)
{
    auto* row = new QWidget;
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    for (QWidget* widget : widgets)
        layout->addWidget(widget);
    return row;
}

// Title and description on the left, the control on the right.
void addRow(QVBoxLayout* layout, QLabel* title, const QString& description, QWidget* control,
            bool line = true)
{
    auto* row = new QHBoxLayout;
    row->setSpacing(16);
    auto* words = new QVBoxLayout;
    words->setSpacing(1);
    words->addWidget(styled(title, "rowTitle"));
    if (!description.isEmpty()) {
        auto* detail = text(description, "desc");
        detail->setWordWrap(true);
        words->addWidget(detail);
    }
    row->addLayout(words, 1);
    if (control)
        row->addWidget(control, 0, Qt::AlignVCenter);
    layout->addLayout(row);
    if (line)
        layout->addWidget(separator());
}

// A small label above its field.
QWidget* field(QLabel* label, QWidget* control)
{
    auto* holder = new QWidget;
    auto* layout = new QVBoxLayout(holder);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(5);
    layout->addWidget(styled(label, "field"));
    layout->addWidget(control);
    return holder;
}

QWidget* column(std::initializer_list<QWidget*> widgets)
{
    auto* holder = new QWidget;
    auto* layout = new QVBoxLayout(holder);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(14);
    for (QWidget* widget : widgets)
        layout->addWidget(widget);
    layout->addStretch();
    return holder;
}

// One page: title, subtitle and actions on top, scrolling content below.
QWidget* page(const QString& title, const QString& subtitle, QWidget* actions, QWidget* content)
{
    auto* holder = new QWidget;
    auto* layout = new QVBoxLayout(holder);
    layout->setContentsMargins(30, 22, 30, 22);
    layout->setSpacing(16);

    auto* header = new QHBoxLayout;
    auto* words = new QVBoxLayout;
    words->setSpacing(0);
    words->addWidget(text(title, "pageTitle"));
    QLabel* pageSub = text(subtitle, "pageSub");
    pageSub->setWordWrap(true);
    words->addWidget(pageSub);
    header->addLayout(words, 1);
    if (actions)
        header->addWidget(actions);
    layout->addLayout(header);

    auto* scroll = new QScrollArea;
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidget(content);
    layout->addWidget(scroll, 1);
    return holder;
}

} // namespace

void SettingsWindow::buildConsoleLayout()
{
    // ── Shop Profile ─────────────────────────────────────────────────────────
    QVBoxLayout* identityBody;
    QFrame* identity = card(&identityBody, tr("Business identity"),
                            tr("How your shop appears on receipts and invoices"));
    identityBody->addWidget(field(ui->shopNameLabel, ui->shopNameField));

    QVBoxLayout* addressBody;
    QFrame* address = card(&addressBody, tr("Address & contact"));
    auto* addressGrid = new QGridLayout;
    addressGrid->setHorizontalSpacing(12);
    addressGrid->setVerticalSpacing(12);
    addressGrid->addWidget(field(ui->adressLable, ui->adressField), 0, 0, 1, 2);
    addressGrid->addWidget(field(new QLabel(tr("City")), ui->cityField), 1, 0);
    addressGrid->addWidget(field(new QLabel(tr("Postal Code")), ui->postalField), 1, 1);
    addressGrid->addWidget(field(ui->phoneLabel, inRow({m_phonePrefix, ui->phoneField})), 2, 0);
    addressGrid->addWidget(field(ui->emailLabel, ui->emailField), 2, 1);
    addressBody->addLayout(addressGrid);

    QVBoxLayout* logoBody;
    QFrame* logo = card(&logoBody, tr("Logo"), tr("Printed on receipts and invoices"));
    logoBody->addWidget(ui->logoImageLabel, 0, Qt::AlignHCenter);

    QVBoxLayout* onlineBody;
    QFrame* online = card(&onlineBody, tr("Online"));
    onlineBody->addWidget(field(ui->websiteLabel, ui->websiteField));
    onlineBody->addWidget(field(ui->facebookLabel, ui->facebookField));
    onlineBody->addWidget(field(ui->instagramLabel, ui->instagramField));
    onlineBody->addWidget(field(ui->tiktokLabel, ui->tiktokField));
    onlineBody->addWidget(field(ui->whatsappLabel, ui->whatsappField));

    auto* shopContent = new QWidget;
    auto* shopColumns = new QHBoxLayout(shopContent);
    shopColumns->setContentsMargins(0, 0, 0, 0);
    shopColumns->setSpacing(16);
    shopColumns->addWidget(column({identity, address}), 3);
    shopColumns->addWidget(column({logo, online}), 2);

    ui->cancelSettingsBtn->setText(tr("Cancel"));
    ui->saveSettingsBtn->setText(tr("Save Changes"));
    ui->saveSettingsBtn->setProperty("btnStyle", "edit");
    m_whiteIcons << NavIcon{ui->saveSettingsBtn, "check"};
    QWidget* shopPage = page(tr("Shop Profile"), tr("Your business details, used on receipts and invoices"),
                             inRow({ui->cancelSettingsBtn, ui->saveSettingsBtn}), shopContent);

    // ── Users & Permissions ─────────────────────────────────────────────────
    ui->usersTable->setShowGrid(false);
    ui->usersTable->setWordWrap(false);
    ui->usersTable->setAlternatingRowColors(false);
    ui->usersTable->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    ui->usersTable->verticalHeader()->setDefaultSectionSize(50);
    ui->usersTable->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    ui->usersTable->setItemDelegateForColumn(1, new AvatarDelegate(ui->usersTable));
    // The cells keep the stored value; the pills show it in the app language.
    auto* rolePills = new PillDelegate(ui->usersTable);
    rolePills->addValue("Admin", PillDelegate::Tone::Blue, tr("Admin"));
    rolePills->addValue("Manager", PillDelegate::Tone::Amber, tr("Manager"));
    rolePills->addValue("Cashier", PillDelegate::Tone::Neutral, tr("Cashier"));
    ui->usersTable->setItemDelegateForColumn(2, rolePills);
    auto* statusPills = new PillDelegate(ui->usersTable);
    statusPills->addValue("Active", PillDelegate::Tone::Green, tr("Active"));
    statusPills->addValue("Inactive", PillDelegate::Tone::Red, tr("Inactive"));
    ui->usersTable->setItemDelegateForColumn(3, statusPills);

    QVBoxLayout* accountsBody;
    QFrame* accounts = card(&accountsBody, tr("Accounts"), tr("Select someone to see their permissions"));
    accountsBody->addWidget(ui->usersTable, 1);

    auto* permissions = new QVBoxLayout;
    permissions->setSpacing(10);
    permissions->setContentsMargins(16, 26, 16, 16);
    const auto permissionRow = [&](QLabel* label, QWidget* box, const QString& detail, bool line = true) {
        label->setText(label->text().remove(':'));
        addRow(permissions, label, detail, box, line);
    };
    permissionRow(ui->pWithdrawLabel, ui->pWithdrawCheck, tr("Take money out from Register"));
    permissionRow(ui->pDeleteTicketLabel, ui->pDeleteTicketCheck, tr("Remove tickets permanently"));
    permissionRow(ui->pDeleteProductLabel, ui->pDeleteProductCheck, tr("Remove items from Inventory"));
    permissionRow(ui->pAddCatLabel, ui->paddCatCheck, tr("Create product categories"));
    permissionRow(ui->pDiscountsLabel, ui->pDiscountsCheck, tr("Create and edit discounts"), false);
    ui->permissionsSaveBtn->setProperty("btnStyle", "edit");
    m_whiteIcons << NavIcon{ui->permissionsSaveBtn, "check"};
    permissions->addWidget(ui->permissionsSaveBtn);
    delete ui->userPermissions->layout();
    ui->userPermissions->setLayout(permissions);

    auto* usersContent = new QWidget;
    auto* usersColumns = new QHBoxLayout(usersContent);
    usersColumns->setContentsMargins(0, 0, 0, 0);
    usersColumns->setSpacing(16);
    usersColumns->addWidget(accounts, 3);
    usersColumns->addWidget(column({ui->userPermissions}), 2);

    ui->userAddBtn->setText(tr("Add User"));
    m_whiteIcons << NavIcon{ui->userAddBtn, "plus"} << NavIcon{ui->userEditBtn, "edit"}
                 << NavIcon{ui->userDeleteBtn, "trash"};
    QWidget* usersPage = page(tr("Users & Permissions"), tr("Who can sign in and what each person can do"),
                              inRow({ui->userDeleteBtn, ui->userEditBtn, ui->userAddBtn}), usersContent);

    // ── Appearance ──────────────────────────────────────────────────────────
    QVBoxLayout* themeBody;
    QFrame* theme = card(&themeBody, tr("Theme"));
    addRow(themeBody, new QLabel(tr("Color theme")), tr("Applies to every POSBox app on this machine"), ui->colorSelect);
    addRow(themeBody, new QLabel(tr("Font size")), QString(), ui->fontSelect);
    ui->bgImageLabel->setText(tr("Background image"));
    addRow(themeBody, ui->bgImageLabel, tr("Shown behind the app windows"),
           inRow({ui->bgImageChooseBtn, ui->bgImageRemoveBtn, ui->bgImagePreview}));
    addRow(themeBody, ui->bgScaleLabel, tr("How the image fills the window"), ui->bgScaleSelect);
    ui->bgDimSlider->setMinimumWidth(160);
    addRow(themeBody, ui->bgDimLabel, tr("Darkens the image so text stays readable"),
           inRow({ui->bgDimSlider, ui->bgDimValue}));
    ui->bgColorLabel->setText(tr("Background color (fallback)"));
    ui->bgColorBtn->setFixedSize(30, 30);
    addRow(themeBody, ui->bgColorLabel, tr("Used when no image is set"),
           inRow({ui->bgColorBtn, ui->bgColorResetBtn}));
    ui->animationsLabel->setText(tr("Enable animations"));
    addRow(themeBody, ui->animationsLabel, tr("Hover, page, dialog and cart motion"), ui->animationsCheckBox, false);
    for (QWidget* combo : {static_cast<QWidget*>(ui->colorSelect), static_cast<QWidget*>(ui->fontSelect),
                           static_cast<QWidget*>(ui->bgScaleSelect)})
        combo->setMinimumWidth(170);
    QWidget* appearancePage = page(tr("Appearance"), tr("Theme, background and motion for this machine"),
                                   nullptr, column({theme}));

    // ── Region & Language ───────────────────────────────────────────────────
    QVBoxLayout* regionBody;
    QFrame* region = card(&regionBody, tr("Region"), tr("Sets the phone and postal code formats and the currency"));
    addRow(regionBody, new QLabel(tr("Country")), QString(), m_countrySelect);
    addRow(regionBody, new QLabel(tr("Currency")), tr("Used for every price and total"), m_currencySelect, false);

    QVBoxLayout* languageBody;
    QFrame* language = card(&languageBody, tr("Language"));
    addRow(languageBody, new QLabel(tr("App language")), tr("Also used on receipts and invoices"),
           m_languageSelect, false);
    QWidget* regionPage = page(tr("Region & Language"), tr("Country, currency and the language POSBox speaks"),
                               nullptr, column({region, language}));

    // ── Register ────────────────────────────────────────────────────────────
    QVBoxLayout* registerBody;
    QFrame* registerCard = card(&registerBody, tr("Register"));
    ui->OOSLabel->setText(tr("Show out of stock items in Register"));
    addRow(registerBody, ui->OOSLabel, tr("Greyed-out tiles stay visible when on"), ui->OOSCheckBox, false);
    QWidget* registerPage = page(tr("Register"), tr("How the Register behaves on this machine"),
                                 nullptr, column({registerCard}));

    // ── Backup & Restore ────────────────────────────────────────────────────
    QVBoxLayout* backupBody;
    QFrame* backup = card(&backupBody, tr("Backup"),
                          tr("Saves the database, logo and product pictures as one zip file"));
    auto* folderRow = new QHBoxLayout;
    folderRow->setSpacing(8);
    folderRow->addWidget(m_syncPathEdit, 1);
    folderRow->addWidget(m_syncPathBtn);
    backupBody->addWidget(styled(new QLabel(tr("Backup folder")), "field"));
    backupBody->addLayout(folderRow);
    auto* backupRow = new QHBoxLayout;
    backupRow->setSpacing(8);
    styled(m_lastBackupLabel, "desc");
    backupRow->addWidget(m_lastBackupLabel, 1);
    backupRow->addWidget(m_backupBtn);
    backupBody->addLayout(backupRow);
    m_textIcons << NavIcon{m_syncPathBtn, "folder"};
    m_whiteIcons << NavIcon{m_backupBtn, "download"};

    QVBoxLayout* restoreBody;
    QFrame* restore = card(&restoreBody, tr("Restore"),
                           tr("Replaces all current data with a backup file. Every POSBox app closes afterwards."));
    auto* restoreRow = new QHBoxLayout;
    restoreRow->addStretch(1);
    restoreRow->addWidget(m_restoreBtn);
    restoreBody->addLayout(restoreRow);
    m_textIcons << NavIcon{m_restoreBtn, "undo"};
    QWidget* backupPage = page(tr("Backup & Restore"), tr("Keep a copy of your data and bring it back"),
                               nullptr, column({backup, restore}));

    // ── Printer ─────────────────────────────────────────────────────────────
    QVBoxLayout* connectionBody;
    QFrame* connection = card(&connectionBody, tr("Connection"));
    ui->printerEnableLabel->setText(tr("Enable Thermal Printer"));
    addRow(connectionBody, ui->printerEnableLabel, QString(), ui->printerEnableBox);
    ui->portSelect->setMinimumWidth(170);
    addRow(connectionBody, new QLabel(tr("Port")), tr("Serial port the printer is connected to"),
           inRow({ui->portSelect, ui->portRefreshBtn}), false);
    m_textIcons << NavIcon{ui->portRefreshBtn, "refresh"};

    QVBoxLayout* serialBody;
    QFrame* serial = card(&serialBody, tr("Serial settings"), tr("Only change these if your printer manual says so"));
    auto* serialGrid = new QGridLayout;
    serialGrid->setHorizontalSpacing(12);
    serialGrid->setVerticalSpacing(12);
    serialGrid->addWidget(field(ui->baudRateLabel, ui->baudRateSelect), 0, 0);
    serialGrid->addWidget(field(ui->dataBitsLabel, ui->dataBitsSelect), 0, 1);
    serialGrid->addWidget(field(ui->parityLabel, ui->paritySelect), 1, 0);
    serialGrid->addWidget(field(ui->stopBitsLabel, ui->stopBitsSelect), 1, 1);
    serialGrid->addWidget(field(ui->flowLabel, ui->flowSelect), 2, 0);
    serialGrid->addWidget(field(ui->latencyLabel, ui->latencySelect), 2, 1);
    serialBody->addLayout(serialGrid);

    QVBoxLayout* paperBody;
    QFrame* paper = card(&paperBody, tr("Paper"));
    auto* paperGrid = new QGridLayout;
    paperGrid->setHorizontalSpacing(12);
    paperGrid->addWidget(field(new QLabel(tr("Paper Size")), ui->paperSizeSelect), 0, 0);
    paperGrid->addWidget(field(new QLabel(tr("Encoding")), ui->encodingSelect), 0, 1);
    paperBody->addLayout(paperGrid);

    auto* printerStatus = new QWidget;
    auto* printerStatusLayout = new QHBoxLayout(printerStatus);
    printerStatusLayout->setContentsMargins(0, 0, 0, 0);
    printerStatusLayout->setSpacing(8);
    styled(ui->printerStatusLabel, "rowTitle");
    printerStatusLayout->addWidget(ui->printerStatusLabel, 1);
    printerStatusLayout->addWidget(ui->printTestBtn);
    printerStatusLayout->addWidget(ui->savePrinterBtn);
    ui->savePrinterBtn->setProperty("btnStyle", "edit");
    m_whiteIcons << NavIcon{ui->savePrinterBtn, "check"};
    m_textIcons << NavIcon{ui->printTestBtn, "printer"};
    QWidget* printerPage = page(tr("Printer"), tr("Thermal receipt printer on this machine"), nullptr,
                                column({connection, serial, paper, printerStatus}));

    // ── Receipt ─────────────────────────────────────────────────────────────
    m_receiptPage = new ReceiptDesignPage(m_api, m_images);
    m_receiptPage->setPaperSize(ui->paperSizeSelect->currentText());
    connect(ui->paperSizeSelect, &QComboBox::currentTextChanged, m_receiptPage, &ReceiptDesignPage::setPaperSize);
    connect(m_receiptPage, &ReceiptDesignPage::paperSizeChosen, ui->paperSizeSelect, &QComboBox::setCurrentText);

    // ── Notifications ───────────────────────────────────────────────────────
    QVBoxLayout* alertsBody;
    QFrame* alerts = card(&alertsBody, tr("Alerts"), tr("Which alerts appear in the notification bell"));
    const auto alert = [&](QLabel* title, const QString& detail, std::initializer_list<QWidget*> controls,
                           QCheckBox* onOff) {
        onOff->setText(QString());
        auto* row = inRow(controls);
        static_cast<QHBoxLayout*>(row->layout())->addWidget(onOff);
        addRow(alertsBody, title, detail, row);
    };
    ui->openTicketLabel->setText(tr("Unpaid Tickets"));
    ui->outStockLabel->setText(tr("Out of Stock"));
    ui->nearOutStockLabel->setText(tr("Nearly Out of Stock"));
    ui->lowStockMinLabel->setText(tr("Low stock threshold"));
    alert(ui->openTicketLabel, tr("Alert after a ticket stays open for this many days"),
          {ui->openTicketDays}, ui->openTicketCheck);
    alert(ui->outStockLabel, tr("When a product reaches zero"), {}, ui->outStockCheck);
    alert(ui->nearOutStockLabel, tr("When stock falls to the threshold"), {}, ui->nearOutStockCheck);
    addRow(alertsBody, ui->lowStockMinLabel, tr("Applies to Nearly Out of Stock and Inventory"), ui->lowStockMinSpin, false);

    QVBoxLayout* soundBody;
    QFrame* sound = card(&soundBody, tr("Sound"));
    ui->notifSoundLabel->setText(tr("Notification sound"));
    addRow(soundBody, ui->notifSoundLabel, tr("A short chime when a new alert appears"),
           inRow({ui->notifSoundTestBtn, ui->notifSoundCheckBox}), false);
    QWidget* alertsPage = page(tr("Notifications"), tr("Which alerts appear and how they sound"),
                               nullptr, column({alerts, sound}));

    // ── Pages and sidebar ───────────────────────────────────────────────────
    auto* pages = new QStackedWidget;
    pages->setObjectName("settingsPages");

    auto* sidebar = new QFrame;
    sidebar->setObjectName("settingsSidebar");
    sidebar->setFixedWidth(256);
    auto* side = new QVBoxLayout(sidebar);
    side->setContentsMargins(14, 18, 14, 14);
    side->setSpacing(3);

    auto* brand = new QHBoxLayout;
    brand->setSpacing(10);
    auto* brandIcon = new QLabel;
    brandIcon->setPixmap(QIcon(":/Icons/images/icons/settings_icon_colored.png").pixmap(36, 36));
    brand->addWidget(brandIcon);
    auto* brandWords = new QVBoxLayout;
    brandWords->setSpacing(0);
    brandWords->addWidget(text(tr("Settings"), "brand"));
    brandWords->addWidget(text(tr("POSBox V%1").arg(POSConfig::APP_VERSION), "desc"));
    brand->addLayout(brandWords, 1);
    side->addLayout(brand);
    side->addSpacing(10);

    auto* navGroup = new QButtonGroup(this);
    const auto section = [&](const QString& name) {
        side->addSpacing(8);
        auto* label = text(name, "cap");
        label->setContentsMargins(10, 0, 0, 3);
        side->addWidget(label);
    };
    const auto item = [&](const QString& iconName, const QString& name, QWidget* content) {
        const int index = pages->addWidget(content);
        auto* button = new QPushButton(name);
        button->setObjectName("settingsNavItem");
        button->setCheckable(true);
        button->setIconSize(QSize(18, 18));
        button->setCursor(Qt::PointingHandCursor);
        navGroup->addButton(button);
        side->addWidget(button);
        m_navIcons << NavIcon{button, iconName};
        connect(button, &QPushButton::clicked, this,
                [pages, index]() { Animations::switchPage(pages, index); });
        if (index == 0)
            button->setChecked(true);
    };
    // "&&" shows a single "&" on a button.
    const auto amp = [](const QString& name) { return QString(name).replace("&", "&&"); };
    section(tr("SHOP"));
    item("store", tr("Shop Profile"), shopPage);
    item("users", amp(tr("Users & Permissions")), usersPage);
    section(tr("APPLICATION"));
    item("palette", tr("Appearance"), appearancePage);
    item("cloud", amp(tr("Region & Language")), regionPage);
    item("grid", tr("Register"), registerPage);
    section(tr("DATA"));
    item("db", amp(tr("Backup & Restore")), backupPage);
    section(tr("DEVICES & ALERTS"));
    item("printer", tr("Printer"), printerPage);
    item("receipt", tr("Receipt"), m_receiptPage);
    item("bell", tr("Notifications"), alertsPage);
    side->addStretch();

    auto* footer = new QHBoxLayout;
    footer->setSpacing(9);
    auto* avatar = new QLabel;
    avatar->setPixmap(avatarPixmap(ui->userLabel->text().section('\n', 0, 0), 34));
    footer->addWidget(avatar);
    styled(ui->userLabel, "desc");
    footer->addWidget(ui->userLabel, 1);
    footer->addWidget(ui->logoutBtn);
    side->addLayout(footer);

    auto* root = new QWidget;
    root->setObjectName("settingsRoot");
    auto* rootLayout = new QHBoxLayout(root);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);
    rootLayout->addWidget(sidebar);
    rootLayout->addWidget(pages, 1);

    // The designer layout stays hidden: every control now lives in the new one.
    QWidget* designed = takeCentralWidget();
    designed->setParent(this);
    designed->hide();
    setCentralWidget(root);

    updateIcons();
}

void SettingsWindow::updateIcons()
{
    const LineIcons::Colors colors = LineIcons::colors();
    for (const NavIcon& item : m_navIcons)
        item.button->setIcon(LineIcons::navIcon(item.name, colors.muted, colors.accent));
    for (const NavIcon& item : m_textIcons)
        item.button->setIcon(LineIcons::icon(item.name, colors.text, 16));
    for (const NavIcon& item : m_whiteIcons)
        item.button->setIcon(LineIcons::icon(item.name, Qt::white, 16));
    m_receiptPage->updateIcons();
}
