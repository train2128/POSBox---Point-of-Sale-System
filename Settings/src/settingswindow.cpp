#include "settingswindow.h"
#include "Prefixes.h"
#include "ui_settingswindow.h"
#include "SettingsConstants.h"
#include "PrinterConstants.h"
#include "StyleManager.h"
#include "UserDialog.h"
#include "UserSession.h"
#include "SessionManager.h"
#include "LocalApi.h"
#include "LocalImages.h"
#include "LocalSettings.h"
#include "NotificationSound.h"
#include "CommonConfig.h"
#include "ThermalPrinter.h"
#include "AccessControl.h"
#include "LogoutManager.h"
#include "AppLock.h"
#include "ChangeNotifier.h"
#include "LocalDb.h"
#include "TranslationManager.h"
#include <QComboBox>
#include <QSet>
#include <QSignalBlocker>
#include <QSqlError>
#include <QSqlQuery>
#include <QApplication>
#include <QCursor>
#include <QLabel>
#include <QHBoxLayout>
#include <QWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QPixmap>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QHeaderView>
#include <QSpinBox>
#include <QTimeEdit>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSerialPortInfo>
#include <QProcess>
#include <QDateTime>
#include <QFormLayout>
#include <QColor>
#include <QColorDialog>


SettingsWindow::SettingsWindow(UserSession* session, LocalApi* api,
                               LocalImages* images, QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::SettingsWindow)
    , m_session(session)
    , m_api(api)
    , m_images(images)
    , m_usersModel(nullptr)
{
    ui->setupUi(this);
    setupUI();
    setupValidators();
    setupConnections();
    loadProfile();
    loadUsers();
    loadAppSettings();
    loadCountryCurrency();
    loadLanguage();
    loadSyncPath();
    populatePrinterCombos();
    scanComPorts();
    loadPrinterSettings();
    loadNotificationSettings();
}

SettingsWindow::~SettingsWindow()
{
    delete ui;
}

// ============================================================================
// SETUP
// ============================================================================

void SettingsWindow::setupUI()
{
    setWindowTitle(tr("POSBox - Settings"));

    if (centralWidget()) {
        centralWidget()->setObjectName("centralwidget");
    }

    // Button labels
    ui->saveSettingsBtn->setText(tr("Save"));
    ui->cancelSettingsBtn->setText(tr("Cancel"));
    ui->logoutBtn->setText("");
    ui->logoutBtn->setIcon(QIcon(StyleManager::iconPath("logout")));
    ui->logoutBtn->setIconSize(QSize(22, 22));
    ui->logoutBtn->setFixedSize(36, 36);
    ui->logoutBtn->setToolTip(tr("Logout"));
    ui->userAddBtn->setText(tr("Add"));
    ui->userEditBtn->setText(tr("Edit"));
    ui->userDeleteBtn->setText(tr("Delete"));
    ui->portRefreshBtn->setText(tr("Refresh"));
    ui->savePrinterBtn->setText(tr("Save"));
    ui->printTestBtn->setText(tr("Test Print"));

    // Label texts
    ui->shopNameLabel->setText(tr("Shop Name"));
    ui->adressLable->setText(tr("Address"));
    ui->phoneLabel->setText(tr("Phone"));
    ui->emailLabel->setText(tr("Email"));
    ui->websiteLabel->setText(tr("Website"));
    ui->facebookLabel->setText(tr("Facebook"));
    ui->instagramLabel->setText(tr("Instagram"));
    ui->tiktokLabel->setText(tr("TikTok"));
    ui->whatsappLabel->setText(tr("WhatsApp"));
    ui->baudRateLabel->setText(tr("Baud Rate"));
    ui->dataBitsLabel->setText(tr("Data Bits"));
    ui->parityLabel->setText(tr("Parity"));
    ui->stopBitsLabel->setText(tr("Stop Bits"));
    ui->flowLabel->setText(tr("Flow Control"));
    ui->latencyLabel->setText(tr("Latency"));
    ui->printerStatusLabel->setText(tr("Status: Not connected"));

    // Allowed day choices for the unpaid-ticket alert
    const int openTicketDays[] = { 3, 7, 15, 30 };
    for (int d : openTicketDays)
        ui->openTicketDays->addItem(tr("%1 days").arg(d), d);

    // Placeholder texts
    ui->shopNameField->setPlaceholderText(tr("Shop name"));
    ui->adressField->setPlaceholderText(tr("Address"));
    ui->cityField->setPlaceholderText(tr("City"));
    ui->emailField->setPlaceholderText(tr("example@domain.com"));
    ui->websiteField->setPlaceholderText(tr("https://"));
    ui->facebookField->setPlaceholderText(tr("Facebook URL"));
    ui->instagramField->setPlaceholderText(tr("Instagram URL"));
    ui->tiktokField->setPlaceholderText(tr("TikTok URL"));
    ui->whatsappField->setPlaceholderText(tr("WhatsApp number"));

    ui->OOSCheckBox->setText("");
    ui->animationsCheckBox->setText("");
    ui->notifSoundCheckBox->setText("");
    ui->notifSoundTestBtn->setText(tr("Play"));
    ui->notifSoundTestBtn->setToolTip(tr("Hear the notification sound"));
    ui->printerEnableBox->setText("");

    // Logo label
    ui->logoImageLabel->setFixedSize(SettingsConstants::LOGO_IMAGE_SIZE,
                                     SettingsConstants::LOGO_IMAGE_SIZE);
    ui->logoImageLabel->setCursor(QCursor(Qt::PointingHandCursor));
    ui->logoImageLabel->setAlignment(Qt::AlignCenter);
    ui->logoImageLabel->setText(tr("Click to\nupload logo"));

    // ---- Users tab ----
    m_usersModel = new QStandardItemModel(0, 5, this);
    m_usersModel->setHorizontalHeaderLabels({
        tr("Username"), tr("Display Name"), tr("Role"), tr("Status"), tr("Last Login")
    });

    ui->usersTable->setModel(m_usersModel);
    ui->usersTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->usersTable->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->usersTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->usersTable->setAlternatingRowColors(true);
    ui->usersTable->verticalHeader()->setVisible(false);
    ui->usersTable->horizontalHeader()->setStretchLastSection(true);
    ui->usersTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->usersTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    ui->usersTable->setShowGrid(false);

    // Button styles
    ui->userAddBtn->setProperty("btnStyle", "add");
    ui->userEditBtn->setProperty("btnStyle", "edit");
    ui->userDeleteBtn->setProperty("btnStyle", "delete");

    // Permissions panel setup
    ui->userPermissions->setTitle(tr("Permissions"));
    ui->pWithdrawLabel->setText(tr("Withdraw Cash"));
    ui->pDeleteTicketLabel->setText(tr("Delete Ticket"));
    ui->pDeleteProductLabel->setText(tr("Delete Products"));
    ui->pAddCatLabel->setText(tr("Add Categories"));
    ui->pDiscountsLabel->setText(tr("Manage Discounts"));
    ui->permissionsSaveBtn->setText(tr("Save Permissions"));

    ui->userPermissions->setVisible(false);

    // Edit/Delete wait for a selection
    ui->userEditBtn->setEnabled(false);
    ui->userDeleteBtn->setEnabled(false);

    // App Settings combos: the stored value travels as item data, so the
    // shown text can be translated.
    ui->colorSelect->addItem(tr("Dark"), "dark");
    ui->colorSelect->addItem(tr("Light"), "light");
    ui->fontSelect->addItem(tr("Small"), "small");
    ui->fontSelect->addItem(tr("Medium"), "medium");
    ui->fontSelect->addItem(tr("Large"), "large");
    ui->fontSelect->setCurrentIndex(1);

    // Background image picker (primary) + color fallback beneath it
    ui->bgImageChooseBtn->setText(tr("Choose Image..."));
    ui->bgImageChooseBtn->setToolTip(
        tr("Pick an image file; it is copied into the app data folder and scaled "
           "to fill the window (light and dark themes)"));
    ui->bgImageRemoveBtn->setText(tr("Remove"));
    ui->bgImageRemoveBtn->setToolTip(tr("Remove the image and fall back to the background color"));
    ui->bgImagePreview->setToolTip(tr("Current background image"));
    ui->bgImagePreview->setScaledContents(false);

    // Fill mode + dim scrim (image only)
    ui->bgScaleSelect->addItem(tr("Cover"), "cover");
    ui->bgScaleSelect->addItem(tr("Stretch"), "stretch");
    ui->bgScaleSelect->addItem(tr("Fit"), "fit");
    ui->bgScaleSelect->setToolTip(
        tr("How the background image fills the window:\n"
           "Cover - fill side to side, crop the overflow (no distortion)\n"
           "Stretch - exact fill, may distort\n"
           "Fit - show the whole image, theme shows around the edges"));
    ui->bgScaleLabel->setText(tr("Scale"));
    ui->bgDimLabel->setText(tr("Dim"));

    ui->bgColorBtn->setToolTip(tr("Pick a background color (used when no image is set)"));
    ui->bgColorBtn->setCursor(Qt::PointingHandCursor);
    ui->bgColorResetBtn->setText(tr("Reset"));
    ui->bgColorResetBtn->setToolTip(tr("Restore the default caramel background"));

    createRegionalControls();
    createBackupControls();

    updateUserDisplay();
    buildConsoleLayout();
    showMaximized();
}

void SettingsWindow::createRegionalControls()
{
    m_countrySelect = new QComboBox(this);
    m_currencySelect = new QComboBox(this);
    m_languageSelect = new QComboBox(this);
    for (QComboBox* combo : {m_countrySelect, m_currencySelect, m_languageSelect})
        combo->setMinimumWidth(220);
    m_phonePrefix = new QLabel(this);
    m_phonePrefix->setObjectName("phonePrefix");
    m_phonePrefix->setMinimumWidth(44);
    m_phonePrefix->setAlignment(Qt::AlignCenter);
    m_currentCountry = CountryData::defaultCountry();
    populateCountryCurrencyCombos();
    populateLanguageCombo();
}

void SettingsWindow::createBackupControls()
{
    m_syncPathEdit = new QLineEdit(this);
    m_syncPathEdit->setReadOnly(true);
    m_syncPathEdit->setPlaceholderText(tr("Choose the folder that receives the backup"));
    m_syncPathBtn = new QPushButton(tr("Browse"), this);
    m_backupBtn = new QPushButton(tr("Back Up Now"), this);
    m_backupBtn->setProperty("btnStyle", "edit");
    m_restoreBtn = new QPushButton(tr("Restore From Backup..."), this);
    m_lastBackupLabel = new QLabel(this);
}

void SettingsWindow::setupValidators()
{
    // Phone and postal rules follow the chosen country (updateRegionalFields).
    updateRegionalFields();
}

void SettingsWindow::setupConnections()
{
    connect(ui->logoutBtn,         &QPushButton::clicked, this, &SettingsWindow::onLogoutClicked);
    connect(ui->saveSettingsBtn,   &QPushButton::clicked, this, &SettingsWindow::onSaveClicked);
    connect(ui->cancelSettingsBtn, &QPushButton::clicked, this, &SettingsWindow::onCancelClicked);

    ui->logoImageLabel->installEventFilter(this);

    connect(ui->phoneField,  &QLineEdit::textChanged, this, &SettingsWindow::onPhoneChanged);
    connect(ui->postalField, &QLineEdit::textChanged, this, &SettingsWindow::onPostalChanged);
    connect(ui->emailField,  &QLineEdit::textChanged, this, &SettingsWindow::onEmailChanged);

    connect(ui->userAddBtn,    &QPushButton::clicked, this, &SettingsWindow::onUserAddClicked);
    connect(ui->userEditBtn,   &QPushButton::clicked, this, &SettingsWindow::onUserEditClicked);
    connect(ui->userDeleteBtn, &QPushButton::clicked, this, &SettingsWindow::onUserDeleteClicked);

    connect(ui->usersTable->selectionModel(), &QItemSelectionModel::currentRowChanged,
            this, &SettingsWindow::onUserSelectionChanged);

    connect(ui->usersTable, &QAbstractItemView::doubleClicked,
            this, &SettingsWindow::onUserEditClicked);

    connect(ui->permissionsSaveBtn, &QPushButton::clicked, this, &SettingsWindow::onPermissionsSaveClicked);

    // Printer
    connect(ui->savePrinterBtn, &QPushButton::clicked, this, &SettingsWindow::onSavePrinterBtn);
    connect(ui->printTestBtn,   &QPushButton::clicked, this, &SettingsWindow::onPrintTestBtn);
    connect(ui->portRefreshBtn, &QPushButton::clicked, this, &SettingsWindow::onPortRefreshBtn);

    // App Settings — auto-save + live-apply on color or font change
    connect(ui->colorSelect, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { saveAppSettings(); });
    connect(ui->fontSelect,  QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { saveAppSettings(); });

    // Background image picker — copies into app data and live-applies
    connect(ui->bgImageChooseBtn, &QPushButton::clicked,
            this, &SettingsWindow::onBgImageChooseClicked);
    connect(ui->bgImageRemoveBtn, &QPushButton::clicked,
            this, &SettingsWindow::onBgImageRemoveClicked);

    // Fill mode + dim slider — saved and re-applied live
    connect(ui->bgScaleSelect, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { applyBackgroundImageOptions(); });
    connect(ui->bgDimSlider, &QSlider::valueChanged,
            this, [this](int) { applyBackgroundImageOptions(); });

    // Background color picker — live-applies on change / reset
    connect(ui->bgColorBtn, &QPushButton::clicked,
            this, &SettingsWindow::onBgColorClicked);
    connect(ui->bgColorResetBtn, &QPushButton::clicked,
            this, [this]() { applyBackgroundColor(QColor()); });

    // Region & language
    connect(m_countrySelect, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsWindow::onCountryChanged);
    connect(m_currencySelect, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsWindow::onCurrencyChanged);
    connect(m_languageSelect, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsWindow::onLanguageChanged);

    // Backup & restore (local, manual)
    connect(m_syncPathBtn, &QPushButton::clicked, this, &SettingsWindow::onSyncPathBrowseClicked);
    connect(m_backupBtn, &QPushButton::clicked, this, &SettingsWindow::onBackupClicked);
    connect(m_restoreBtn, &QPushButton::clicked, this, &SettingsWindow::onImportClicked);

    // OOS checkbox — auto-save immediately on toggle
    connect(ui->OOSCheckBox, &QCheckBox::toggled, this, [this](bool) { saveAppSettings(); });
    connect(ui->animationsCheckBox, &QCheckBox::toggled, this, [this](bool) { saveAppSettings(); });
    connect(ui->notifSoundCheckBox, &QCheckBox::toggled, this, [this](bool) { saveAppSettings(); });
    connect(ui->notifSoundTestBtn, &QPushButton::clicked, this, []() { NotificationSound::preview(); });

    // Printer enable checkbox
    connect(ui->printerEnableBox, &QCheckBox::toggled,
            this, &SettingsWindow::onPrinterEnableToggled);

    // Notifications — auto-save on any change
    connect(ui->openTicketCheck, &QCheckBox::toggled, this, [this](bool) { saveNotificationSettings(); });
    connect(ui->openTicketDays, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { saveNotificationSettings(); });
    connect(ui->outStockCheck, &QCheckBox::toggled, this, [this](bool) { saveNotificationSettings(); });
    connect(ui->nearOutStockCheck, &QCheckBox::toggled, this, [this](bool) { saveNotificationSettings(); });
    connect(ui->lowStockMinSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) { saveNotificationSettings(); });
}

void SettingsWindow::updateUserDisplay()
{
    if (m_session) {
        ui->userLabel->setText(QString("%1\n%2")
                                   .arg(m_session->displayName())
                                   .arg(AppHelpers::roleName(m_session->role())));
    }
}

// ============================================================================
// EVENT FILTER — logo label click
// ============================================================================

bool SettingsWindow::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == ui->logoImageLabel && event->type() == QEvent::MouseButtonPress) {
        onLogoClicked();
        return true;
    }
    return QMainWindow::eventFilter(obj, event);
}

// ============================================================================
// SHOP PROFILE
// ============================================================================

void SettingsWindow::loadProfile()
{
    QList<DataRow> rows = m_api->select("shop_profile", {{"id", 1}});
    if (!m_api->ok() || rows.isEmpty()) {
        qWarning() << "Failed to load shop profile:" << m_api->lastError();
        return;
    }
    DataRow query = rows.first();

    ui->shopNameField->setText(query.value("shop_name").toString());
    ui->adressField->setText(query.value("address").toString());
    ui->cityField->setText(query.value("city").toString());
    ui->postalField->setText(query.value("postal").toString());
    ui->emailField->setText(query.value("email").toString());
    ui->websiteField->setText(query.value("website").toString());
    ui->facebookField->setText(query.value("facebook").toString());
    ui->instagramField->setText(query.value("instagram").toString());
    ui->tiktokField->setText(query.value("tiktok").toString());
    ui->whatsappField->setText(query.value("whatsapp").toString());

    QString phoneRaw = query.value("phone").toString();
    QString prefix   = m_currentCountry.phoneCode;
    if (phoneRaw.startsWith(prefix)) {
        phoneRaw = phoneRaw.mid(prefix.length());
    }
    ui->phoneField->setText(phoneRaw);

    m_existingLogoFilename = query.value("logo_path").toString();
    m_newLogoSourcePath.clear();

    if (!m_existingLogoFilename.isEmpty()) {
        QByteArray raw = m_api->downloadImage(m_existingLogoFilename);
        if (!raw.isEmpty()) {
            QPixmap pixmap;
            if (pixmap.loadFromData(raw))
                displayPixmap(pixmap);
            else
                showLogoPlaceholder();
        } else {
            showLogoPlaceholder();
        }
    } else {
        showLogoPlaceholder();
    }

    onPhoneChanged(ui->phoneField->text());
    onPostalChanged(ui->postalField->text());
    onEmailChanged(ui->emailField->text());
}

bool SettingsWindow::saveProfile()
{
    if (!validateFields()) return false;

    QString logoFilenameToSave = m_existingLogoFilename;

    if (!m_newLogoSourcePath.isEmpty()) {
        // The logo is copied into the app-data logo folder; only the file
        // name is stored.
        QFile src(m_newLogoSourcePath);
        QString fileName = QFileInfo(m_newLogoSourcePath).fileName();
        if (src.open(QIODevice::ReadOnly)
            && m_api->uploadImage(fileName, src.readAll(), LocalApi::ImageKind::Logo)) {
            logoFilenameToSave     = fileName;
            m_existingLogoFilename = fileName;
            m_newLogoSourcePath.clear();
            if (m_images) m_images->clear();
            qDebug() << "Logo saved as:" << fileName;
        } else {
            qWarning() << "Failed to upload logo:" << m_api->lastError();
            AppHelpers::showWarning(this, tr("Could not save the logo image.") + "\n\n" + m_api->lastError(), tr("Logo"));
            return false;
        }
    }

    QString fullPhone;
    if (!ui->phoneField->text().trimmed().isEmpty()) {
        fullPhone = m_currentCountry.phoneCode + ui->phoneField->text().trimmed();
    }

    auto optText = [](const QString& t) -> QVariant {
        return t.trimmed().isEmpty() ? QVariant() : QVariant(t.trimmed());
    };
    QVariantMap values;
    values["shop_name"] = ui->shopNameField->text().trimmed();
    values["address"] = optText(ui->adressField->text());
    values["postal"] = optText(ui->postalField->text());
    values["city"] = optText(ui->cityField->text());
    values["country"] = m_currentCountry.code.isEmpty() ? QVariant() : QVariant(m_currentCountry.code);
    values["phone"] = fullPhone.isEmpty() ? QVariant() : QVariant(fullPhone);
    values["email"] = optText(ui->emailField->text());
    values["website"] = optText(ui->websiteField->text());
    values["facebook"] = optText(ui->facebookField->text());
    values["instagram"] = optText(ui->instagramField->text());
    values["tiktok"] = optText(ui->tiktokField->text());
    values["whatsapp"] = optText(ui->whatsappField->text());
    values["business_mode"] = QVariant();
    values["currency"] = QString::fromUtf8(AppConstants::CURRENCY);
    values["tax_rate"] = QVariant();
    values["logo_path"] = logoFilenameToSave.isEmpty() ? QVariant() : QVariant(logoFilenameToSave);

    if (!m_api->update("shop_profile", values, {{"id", 1}})) {
        qWarning() << "Failed to save profile:" << m_api->lastError();
        AppHelpers::showWarning(this, AppHelpers::trMsg(SettingsConstants::MSG_SAVE_FAILED), tr("Save Failed"));
        return false;
    }

    AppHelpers::showInfo(this, AppHelpers::trMsg(SettingsConstants::MSG_SAVE_SUCCESS), tr("Saved"));
    return true;
}

bool SettingsWindow::validateFields()
{
    if (ui->shopNameField->text().trimmed().isEmpty()) {
        AppHelpers::showWarning(this, AppHelpers::trMsg(SettingsConstants::MSG_SHOP_NAME_EMPTY));
        ui->shopNameField->setFocus();
        return false;
    }
    if (!ui->phoneField->text().trimmed().isEmpty() && !isPhoneValid()) {
        AppHelpers::showWarning(this, AppHelpers::trMsg(SettingsConstants::MSG_PHONE_INVALID).arg(m_currentCountry.phoneDigits));
        ui->phoneField->setFocus();
        return false;
    }
    if (!ui->postalField->text().trimmed().isEmpty() && !isPostalValid()) {
        AppHelpers::showWarning(this, AppHelpers::trMsg(SettingsConstants::MSG_POSTAL_INVALID).arg(m_currentCountry.postalDigits));
        ui->postalField->setFocus();
        return false;
    }
    if (!ui->emailField->text().trimmed().isEmpty() && !isEmailValid()) {
        AppHelpers::showWarning(this, AppHelpers::trMsg(SettingsConstants::MSG_EMAIL_INVALID));
        ui->emailField->setFocus();
        return false;
    }
    return true;
}

bool SettingsWindow::isPhoneValid() const
{
    QRegularExpression rx(QString("^\\d{%1}$").arg(m_currentCountry.phoneDigits));
    return rx.match(ui->phoneField->text().trimmed()).hasMatch();
}

bool SettingsWindow::isPostalValid() const
{
    if (m_currentCountry.postalDigits <= 0)
        return !ui->postalField->text().trimmed().isEmpty();
    QRegularExpression rx(QString("^\\d{%1}$").arg(m_currentCountry.postalDigits));
    return rx.match(ui->postalField->text().trimmed()).hasMatch();
}

bool SettingsWindow::isEmailValid() const
{
    QRegularExpression rx(R"(^[a-zA-Z0-9._%+\-]+@[a-zA-Z0-9.\-]+\.[a-zA-Z]{2,}$)");
    return rx.match(ui->emailField->text().trimmed()).hasMatch();
}

void SettingsWindow::setFieldState(QLineEdit* field, bool valid)
{
    field->setProperty("state", valid ? "valid" : "invalid");
    field->style()->unpolish(field);
    field->style()->polish(field);
    field->update();
}

void SettingsWindow::clearFieldState(QLineEdit* field)
{
    field->setProperty("state", QVariant());
    field->style()->unpolish(field);
    field->style()->polish(field);
    field->update();
}

void SettingsWindow::onPhoneChanged(const QString& text)
{
    if (text.trimmed().isEmpty()) { clearFieldState(ui->phoneField); return; }
    setFieldState(ui->phoneField, isPhoneValid());
}

void SettingsWindow::onPostalChanged(const QString& text)
{
    if (text.trimmed().isEmpty()) { clearFieldState(ui->postalField); return; }
    setFieldState(ui->postalField, isPostalValid());
}

void SettingsWindow::onEmailChanged(const QString& text)
{
    if (text.trimmed().isEmpty()) { clearFieldState(ui->emailField); return; }
    setFieldState(ui->emailField, isEmailValid());
}

void SettingsWindow::onLogoClicked()
{
    QString filePath = QFileDialog::getOpenFileName(
        this, tr("Select Logo Image"), QString(),
        tr("Images (*.png *.jpg *.jpeg *.bmp)"));
    if (filePath.isEmpty()) return;
    m_newLogoSourcePath = filePath;
    QPixmap pixmap(filePath);
    if (pixmap.isNull()) { qWarning() << "Failed to load logo:" << filePath; return; }
    displayPixmap(pixmap);
}

void SettingsWindow::displayPixmap(const QPixmap& pixmap)
{
    if (pixmap.isNull()) { showLogoPlaceholder(); return; }
    ui->logoImageLabel->setPixmap(
        pixmap.scaled(ui->logoImageLabel->size(),
                      Qt::KeepAspectRatio, Qt::SmoothTransformation));
    ui->logoImageLabel->setText(QString());
}

void SettingsWindow::showLogoPlaceholder()
{
    ui->logoImageLabel->setPixmap(QPixmap());
    ui->logoImageLabel->setText(tr("Click to\nupload logo"));
}

void SettingsWindow::onSaveClicked()   { saveProfile(); }
void SettingsWindow::onCancelClicked() { loadProfile(); }

// ============================================================================
// USERS
// ============================================================================

void SettingsWindow::loadUsers()
{
    m_usersModel->removeRows(0, m_usersModel->rowCount());

    QList<DataRow> rows = m_api->select("accounts", {}, "username");
    if (!m_api->ok()) {
        qWarning() << "Failed to load users:" << m_api->lastError();
        return;
    }

    for (const DataRow& r : rows) {
        QString username    = r.value("username").toString();
        QString displayName = r.value("display_name").toString();
        QString role        = r.value("role").toString();
        bool    isActive    = r.value("is_active").toBool();
        QString lastLogin   = r.value("last_login").toString();

        QList<QStandardItem*> row;

        auto makeItem = [&](const QString& text) -> QStandardItem* {
            QStandardItem* item = new QStandardItem(text);
            item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            item->setData(username, Qt::UserRole);
            if (!isActive) {
                item->setForeground(QBrush(QColor("#666666")));
            }
            return item;
        };

        row << makeItem(username);
        row << makeItem(displayName);
        row << makeItem(role);
        row << makeItem(isActive ? "Active" : "Inactive");
        row << makeItem(lastLogin.isEmpty() ? tr("Never") : lastLogin);

        m_usersModel->appendRow(row);
    }

    qDebug() << "Loaded" << m_usersModel->rowCount() << "users";
}

QString SettingsWindow::selectedUsername() const
{
    QModelIndexList selected = ui->usersTable->selectionModel()->selectedRows();
    if (selected.isEmpty()) return QString();
    return m_usersModel->item(selected.first().row(), 0)->text();
}

void SettingsWindow::onUserAddClicked()
{
    UserDialog dialog(m_api, UserDialog::Mode::Add, QString(), this);
    if (dialog.exec() == QDialog::Accepted) {
        AppHelpers::showInfo(this, AppHelpers::trMsg(SettingsConstants::MSG_USER_ADD_SUCCESS), tr("Success"));
        loadUsers();
    }
}

void SettingsWindow::onUserEditClicked()
{
    QString username = selectedUsername();
    if (username.isEmpty()) {
        AppHelpers::showWarning(this, AppHelpers::trMsg(SettingsConstants::MSG_USER_SELECT), tr("No Selection"));
        return;
    }

    UserDialog dialog(m_api, UserDialog::Mode::Edit, username, this);
    if (dialog.exec() == QDialog::Accepted) {
        AppHelpers::showInfo(this, AppHelpers::trMsg(SettingsConstants::MSG_USER_EDIT_SUCCESS), tr("Success"));
        loadUsers();
    }
}

void SettingsWindow::onUserDeleteClicked()
{
    QString username = selectedUsername();
    if (username.isEmpty()) {
        AppHelpers::showWarning(this, AppHelpers::trMsg(SettingsConstants::MSG_USER_SELECT), tr("No Selection"));
        return;
    }

    if (username == m_session->username()) {
        AppHelpers::showWarning(this, AppHelpers::trMsg(SettingsConstants::MSG_USER_CANNOT_DELETE_SELF), tr("Not Allowed"));
        return;
    }

    QList<DataRow> admins = m_api->select("accounts", {{"role", "Admin"}, {"is_active", 1}});
    int adminCount = m_api->ok() ? admins.size() : 0;

    QList<DataRow> target = m_api->select("accounts", {{"username", username}});
    if (m_api->ok() && !target.isEmpty()) {
        QString role = target.first().value("role").toString();
        if (role == "Admin" && adminCount <= 1) {
            AppHelpers::showWarning(this, AppHelpers::trMsg(SettingsConstants::MSG_USER_CANNOT_DELETE_LAST_ADMIN), tr("Not Allowed"));
            return;
        }
    }

    if (!AppHelpers::confirmAction(this, AppHelpers::trMsg(SettingsConstants::MSG_USER_DELETE_CONFIRM))) return;

    if (!m_api->remove("accounts", {{"username", username}})) {
        qWarning() << "Failed to delete user:" << m_api->lastError();
        AppHelpers::showWarning(this, tr("Failed to delete user."));
        return;
    }

    AppHelpers::showInfo(this, AppHelpers::trMsg(SettingsConstants::MSG_USER_DELETE_SUCCESS), tr("Deleted"));
    loadUsers();
}

void SettingsWindow::onUserSelectionChanged(const QModelIndex& current, const QModelIndex& /*previous*/)
{
    if (!current.isValid()) {
        ui->userEditBtn->setEnabled(false);
        ui->userDeleteBtn->setEnabled(false);
        clearPermissionsPanel();
        return;
    }

    QString username = m_usersModel->item(current.row(), 0)->text();

    ui->userEditBtn->setEnabled(true);
    ui->userDeleteBtn->setEnabled(true);

    updatePermissionsPanel(username);
}

void SettingsWindow::updatePermissionsPanel(const QString& username)
{
    if (username.isEmpty()) {
        clearPermissionsPanel();
        return;
    }

    QList<DataRow> rows = m_api->select("accounts", {{"username", username}});
    if (!m_api->ok() || rows.isEmpty()) {
        clearPermissionsPanel();
        return;
    }

    QString role = rows.first().value("role").toString();

    if (AccessControl::isAdminRole(role)) {
        ui->userPermissions->setVisible(true);
        ui->userPermissions->setTitle(tr("Permissions (Admin has all permissions)"));
        ui->pWithdrawCheck->setChecked(true);
        ui->pWithdrawCheck->setEnabled(false);
        ui->pDeleteTicketCheck->setChecked(true);
        ui->pDeleteTicketCheck->setEnabled(false);
        ui->pDeleteProductCheck->setChecked(true);
        ui->pDeleteProductCheck->setEnabled(false);
        ui->paddCatCheck->setChecked(true);
        ui->paddCatCheck->setEnabled(false);
        ui->pDiscountsCheck->setChecked(true);
        ui->pDiscountsCheck->setEnabled(false);
        ui->permissionsSaveBtn->setEnabled(false);
        return;
    }

    ui->userPermissions->setVisible(true);
    ui->userPermissions->setTitle(tr("Permissions for %1 (%2)").arg(username, AppHelpers::roleName(role)));
    ui->permissionsSaveBtn->setEnabled(true);

    bool isManager = (role == "Manager");

    ui->pWithdrawCheck->setEnabled(true);
    ui->pDeleteTicketCheck->setEnabled(true);
    ui->pDeleteProductCheck->setEnabled(isManager);
    ui->paddCatCheck->setEnabled(isManager);
    ui->pDiscountsCheck->setEnabled(isManager);

    loadUserPermissions(username);
}

void SettingsWindow::loadUserPermissions(const QString& username)
{
    QMap<QString, bool> permissions;
    QList<DataRow> rows = m_api->select("user_permissions", {{"username", username}});
    if (m_api->ok()) {
        for (const DataRow& row : rows)
            permissions[row.value("permission_key").toString()] =
                row.value("is_allowed").toInt() == 1;
    }

    ui->pWithdrawCheck->setChecked(permissions.value(AccessControl::PERMISSION_WITHDRAW_CASH, false));
    ui->pDeleteTicketCheck->setChecked(permissions.value(AccessControl::PERMISSION_DELETE_TICKET, false));
    ui->pDeleteProductCheck->setChecked(permissions.value(AccessControl::PERMISSION_DELETE_PRODUCT, false));
    ui->paddCatCheck->setChecked(permissions.value(AccessControl::PERMISSION_ADD_CATEGORY, false));
    ui->pDiscountsCheck->setChecked(permissions.value(AccessControl::PERMISSION_MANAGE_DISCOUNTS, false));
}

void SettingsWindow::saveUserPermissions(const QString& username)
{
    if (username.isEmpty() || AccessControl::isAdminRole(username)) {
        return;
    }

    const QList<QPair<QString, bool>> perms = {
        {AccessControl::PERMISSION_WITHDRAW_CASH, ui->pWithdrawCheck->isChecked()},
        {AccessControl::PERMISSION_DELETE_TICKET, ui->pDeleteTicketCheck->isChecked()},
        {AccessControl::PERMISSION_DELETE_PRODUCT, ui->pDeleteProductCheck->isChecked()},
        {AccessControl::PERMISSION_ADD_CATEGORY, ui->paddCatCheck->isChecked()},
        {AccessControl::PERMISSION_MANAGE_DISCOUNTS, ui->pDiscountsCheck->isChecked()},
    };
    // Upsert per key: update, insert if missing.
    for (const auto& [key, allowed] : perms) {
        QVariantMap where{{"username", username}, {"permission_key", key}};
        if (!m_api->update("user_permissions", {{"is_allowed", allowed ? 1 : 0}}, where)
            || !m_api->ok()) {
            qWarning() << "Failed to save permission:" << key << m_api->lastError();
            continue;
        }
        QList<DataRow> check = m_api->select("user_permissions", where);
        if (m_api->ok() && check.isEmpty()) {
            QVariantMap values{{"username", username},
                               {"permission_key", key},
                               {"is_allowed", allowed ? 1 : 0}};
            if (m_api->insert("user_permissions", values) < 0)
                qWarning() << "Failed to insert permission:" << key << m_api->lastError();
        }
    }
}

void SettingsWindow::clearPermissionsPanel()
{
    ui->userPermissions->setVisible(false);
    ui->pWithdrawCheck->setChecked(false);
    ui->pDeleteTicketCheck->setChecked(false);
    ui->pDeleteProductCheck->setChecked(false);
    ui->paddCatCheck->setChecked(false);
    ui->pDiscountsCheck->setChecked(false);
}

void SettingsWindow::onPermissionsSaveClicked()
{
    QString username = selectedUsername();
    if (username.isEmpty()) {
        return;
    }

    saveUserPermissions(username);
    AppHelpers::showInfo(this, tr("Permissions saved successfully."), tr("Success"));
}

// ============================================================================
// LOGOUT
// ============================================================================

void SettingsWindow::onLogoutClicked()
{
    if (!LogoutManager::confirmAndLogout(this, m_session->sessionId())) {
        return;
    }
    close();
    emit logoutRequested();
}

// ============================================================================
// PRINTER TAB — SETUP
// ============================================================================

void SettingsWindow::populatePrinterCombos()
{
    // Baud Rate — all standard baud rates lowest to highest
    ui->baudRateSelect->clear();
    const QList<int> baudRates = {
        110, 300, 600, 1200, 2400, 4800, 9600,
        14400, 19200, 38400, 57600, 115200, 128000, 256000
    };
    for (int rate : baudRates)
        ui->baudRateSelect->addItem(QString::number(rate), rate);

    // Data Bits
    ui->dataBitsSelect->clear();
    for (int bits : {5, 6, 7, 8})
        ui->dataBitsSelect->addItem(QString::number(bits), bits);

    // Parity
    ui->paritySelect->clear();
    ui->paritySelect->addItems({"None", "Odd", "Even", "Mark", "Space"});

    // Stop Bits
    ui->stopBitsSelect->clear();
    ui->stopBitsSelect->addItems({"1", "1.5", "2"});

    // Flow Control
    ui->flowSelect->clear();
    ui->flowSelect->addItems({"None", "Hardware", "Software"});

    // Latency spinbox range
    ui->latencySelect->setMinimum(PrinterConstants::LATENCY_MIN);
    ui->latencySelect->setMaximum(PrinterConstants::LATENCY_MAX);
    ui->latencySelect->setSuffix(tr(" ms"));

    // Paper Size — all common thermal printer widths
    ui->paperSizeSelect->clear();
    ui->paperSizeSelect->addItems({"58mm", "76mm", "80mm", "82.5mm", "104mm", "112mm"});

    // Encoding
    ui->encodingSelect->clear();
    for (const char* name : {"UTF-8", "IBM850", "CP1252", "GB2312"})
        ui->encodingSelect->addItem(QString::fromLatin1(name), QString::fromLatin1(name));
    // Any script (Arabic, Cyrillic, Chinese...) printed as pictures.
    ui->encodingSelect->addItem(tr("Image (any language)"), "IMAGE");
}

void SettingsWindow::scanComPorts()
{
    const QString currentPort = ui->portSelect->currentText();

    ui->portSelect->clear();

    const QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : ports)
        ui->portSelect->addItem(info.portName());

    if (ui->portSelect->count() == 0) {
        ui->printerStatusLabel->setText(AppHelpers::trMsg(SettingsConstants::MSG_PRINTER_NO_PORTS_FOUND));
    } else {
        const int idx = ui->portSelect->findText(currentPort);
        if (idx >= 0)
            ui->portSelect->setCurrentIndex(idx);

        ui->printerStatusLabel->clear();
    }
}

// ============================================================================
// PRINTER TAB — LOAD / SAVE
// ============================================================================

void SettingsWindow::loadPrinterSettings()
{
    const int defaultBaudIdx = ui->baudRateSelect->findData(PrinterConstants::DEFAULT_BAUD_RATE);
    if (defaultBaudIdx >= 0) ui->baudRateSelect->setCurrentIndex(defaultBaudIdx);

    const int defaultBitsIdx = ui->dataBitsSelect->findData(PrinterConstants::DEFAULT_DATA_BITS);
    if (defaultBitsIdx >= 0) ui->dataBitsSelect->setCurrentIndex(defaultBitsIdx);

    ui->paritySelect->setCurrentText(PrinterConstants::DEFAULT_PARITY);
    ui->stopBitsSelect->setCurrentText(PrinterConstants::DEFAULT_STOP_BITS);
    ui->flowSelect->setCurrentText(PrinterConstants::DEFAULT_FLOW_CONTROL);
    ui->latencySelect->setValue(PrinterConstants::DEFAULT_LATENCY);
    ui->paperSizeSelect->setCurrentText(PrinterConstants::DEFAULT_PAPER_SIZE);
    ui->encodingSelect->setCurrentIndex(qMax(0, ui->encodingSelect->findData(QString(PrinterConstants::DEFAULT_ENCODING))));

    {
        QSignalBlocker block(ui->printerEnableBox);
        ui->printerEnableBox->setChecked(PrinterConstants::DEFAULT_ENABLED);
    }
    setPrinterControlsEnabled(PrinterConstants::DEFAULT_ENABLED);

    // Printer config is a per-machine setting.
    const QVariantMap printer = LocalSettings::load().printer;
    const bool enabled = printer.value("enabled", 0).toInt() != 0;
    {
        QSignalBlocker block(ui->printerEnableBox);
        ui->printerEnableBox->setChecked(enabled);
    }
    setPrinterControlsEnabled(enabled);

    const QString savedPort = printer.value("port").toString();
    const int portIdx = ui->portSelect->findText(savedPort);
    if (portIdx >= 0)
        ui->portSelect->setCurrentIndex(portIdx);

    const int savedBaud = printer.value("baud_rate", PrinterConstants::DEFAULT_BAUD_RATE).toInt();
    const int baudIdx = ui->baudRateSelect->findData(savedBaud);
    if (baudIdx >= 0) ui->baudRateSelect->setCurrentIndex(baudIdx);

    const int savedBits = printer.value("data_bits", PrinterConstants::DEFAULT_DATA_BITS).toInt();
    const int bitsIdx = ui->dataBitsSelect->findData(savedBits);
    if (bitsIdx >= 0) ui->dataBitsSelect->setCurrentIndex(bitsIdx);

    ui->paritySelect->setCurrentText(printer.value("parity", PrinterConstants::DEFAULT_PARITY).toString());
    ui->stopBitsSelect->setCurrentText(printer.value("stop_bits", PrinterConstants::DEFAULT_STOP_BITS).toString());
    ui->flowSelect->setCurrentText(printer.value("flow_control", PrinterConstants::DEFAULT_FLOW_CONTROL).toString());
    ui->latencySelect->setValue(printer.value("latency_ms", PrinterConstants::DEFAULT_LATENCY).toInt());
    ui->paperSizeSelect->setCurrentText(printer.value("paper_size", PrinterConstants::DEFAULT_PAPER_SIZE).toString());
    {
        const int index = ui->encodingSelect->findData(
            printer.value("encoding", PrinterConstants::DEFAULT_ENCODING).toString());
        if (index >= 0)
            ui->encodingSelect->setCurrentIndex(index);
    }
}

void SettingsWindow::savePrinterSettings()
{
    LocalSettings local = LocalSettings::load();
    QVariantMap printer = local.printer;
    printer["enabled"] = ui->printerEnableBox->isChecked() ? 1 : 0;
    printer["port"] = ui->portSelect->currentText();
    printer["baud_rate"] = ui->baudRateSelect->currentData().toInt();
    printer["data_bits"] = ui->dataBitsSelect->currentData().toInt();
    printer["parity"] = ui->paritySelect->currentText();
    printer["stop_bits"] = ui->stopBitsSelect->currentText();
    printer["flow_control"] = ui->flowSelect->currentText();
    printer["latency_ms"] = ui->latencySelect->value();
    printer["paper_size"] = ui->paperSizeSelect->currentText();
    printer["encoding"] = ui->encodingSelect->currentData().toString();
    local.printer = printer;

    if (local.save()) {
        ui->printerStatusLabel->setText(AppHelpers::trMsg(SettingsConstants::MSG_PRINTER_SAVE_SUCCESS));
    } else {
        ui->printerStatusLabel->setText(AppHelpers::trMsg(SettingsConstants::MSG_PRINTER_SAVE_FAILED));
    }
}

// ============================================================================
// PRINTER TAB — SLOTS
// ============================================================================

void SettingsWindow::onSavePrinterBtn()
{
    savePrinterSettings();
}

void SettingsWindow::onPortRefreshBtn()
{
    scanComPorts();
    loadPrinterSettings();
}

void SettingsWindow::onPrinterEnableToggled(bool enabled)
{
    setPrinterControlsEnabled(enabled);
    // Auto-save the enabled state immediately so other apps can read it
    savePrinterSettings();

    if (!enabled)
        ui->printerStatusLabel->setText(AppHelpers::trMsg(SettingsConstants::MSG_PRINTER_DISABLED));
    else
        ui->printerStatusLabel->clear();
}

void SettingsWindow::setPrinterControlsEnabled(bool enabled)
{
    // Every printer configuration widget below the enable checkbox
    // is enabled or disabled as a group.
    ui->portSelect->setEnabled(enabled);
    ui->portRefreshBtn->setEnabled(enabled);
    ui->baudRateSelect->setEnabled(enabled);
    ui->baudRateLabel->setEnabled(enabled);
    ui->dataBitsSelect->setEnabled(enabled);
    ui->dataBitsLabel->setEnabled(enabled);
    ui->paritySelect->setEnabled(enabled);
    ui->parityLabel->setEnabled(enabled);
    ui->stopBitsSelect->setEnabled(enabled);
    ui->stopBitsLabel->setEnabled(enabled);
    ui->flowSelect->setEnabled(enabled);
    ui->flowLabel->setEnabled(enabled);
    ui->latencySelect->setEnabled(enabled);
    ui->latencyLabel->setEnabled(enabled);
    ui->paperSizeSelect->setEnabled(enabled);
    ui->encodingSelect->setEnabled(enabled);
    ui->savePrinterBtn->setEnabled(enabled);
    ui->printTestBtn->setEnabled(enabled);
}

void SettingsWindow::onPrintTestBtn()
{
    if (!ui->printerEnableBox->isChecked()) {
        ui->printerStatusLabel->setText(AppHelpers::trMsg(SettingsConstants::MSG_PRINTER_DISABLED));
        return;
    }

    if (ui->portSelect->currentText().isEmpty()) {
        ui->printerStatusLabel->setText(AppHelpers::trMsg(SettingsConstants::MSG_PRINTER_NO_PORT));
        return;
    }

    if (!m_api) {
        ui->printerStatusLabel->setText(AppHelpers::trMsg(SettingsConstants::MSG_PRINTER_TEST_FAILED));
        return;
    }

    const ThermalPrinter::TicketData data = Receipt::sampleTicket(m_api, m_images);

    ThermalPrinter printer(LocalSettings::load().printer);
    const bool ok = printer.print(data);

    ui->printerStatusLabel->setText(
        ok ? AppHelpers::trMsg(SettingsConstants::MSG_PRINTER_TEST_SENT)
           : AppHelpers::trMsg(SettingsConstants::MSG_PRINTER_TEST_FAILED));
}
// ============================================================================
// APP SETTINGS — THEME
// ============================================================================

void SettingsWindow::loadAppSettings()
{
    // Block signals so setting combos don't trigger saveAppSettings
    QSignalBlocker blockColor(ui->colorSelect);
    QSignalBlocker blockFont(ui->fontSelect);
    QSignalBlocker blockOOS(ui->OOSCheckBox);
    QSignalBlocker blockAnimations(ui->animationsCheckBox);
    QSignalBlocker blockNotifSound(ui->notifSoundCheckBox);
    QSignalBlocker blockScale(ui->bgScaleSelect);
    QSignalBlocker blockDim(ui->bgDimSlider);

    LocalSettings local = LocalSettings::load();
    ui->colorSelect->setCurrentIndex(qMax(0, ui->colorSelect->findData(
        local.colorScheme == "light" ? "light" : "dark")));
    ui->fontSelect->setCurrentIndex(qMax(0, ui->fontSelect->findData(local.fontSize)));

    ui->OOSCheckBox->setChecked(local.showOosProducts);
    ui->animationsCheckBox->setChecked(local.animationsEnabled);
    ui->notifSoundCheckBox->setChecked(local.notificationSound);
    ui->bgScaleSelect->setCurrentIndex(
        local.backgroundScale == "stretch" ? 1
        : local.backgroundScale == "fit"   ? 2
                                           : 0);
    ui->bgDimSlider->setValue(local.backgroundDim);
    ui->bgDimValue->setText(QString("%1%").arg(local.backgroundDim));
    updateBgSwatch(local.backgroundColor);
    updateBgImagePreview();
}

namespace {
StyleManager::ColorScheme settingsColor(const QString& key)
{
    return (key == "light") ? StyleManager::ColorScheme::Light
                            : StyleManager::ColorScheme::Dark;
}
StyleManager::FontSize settingsFont(const QString& key)
{
    if (key == "medium") return StyleManager::FontSize::Medium;
    if (key == "large")  return StyleManager::FontSize::Large;
    return StyleManager::FontSize::Small;
}
QString settingsColorName(StyleManager::ColorScheme color)
{
    return (color == StyleManager::ColorScheme::Light) ? "light" : "dark";
}
QString settingsFontName(StyleManager::FontSize font)
{
    switch (font) {
    case StyleManager::FontSize::Medium: return "medium";
    case StyleManager::FontSize::Large:  return "large";
    default:                             return "small";
    }
}
} // namespace

void SettingsWindow::saveAppSettings()
{
    LocalSettings local = LocalSettings::load();

    const StyleManager::ColorScheme color = settingsColor(ui->colorSelect->currentData().toString());
    const StyleManager::FontSize font = settingsFont(ui->fontSelect->currentData().toString());

    local.colorScheme = settingsColorName(color);
    local.fontSize = settingsFontName(font);
    local.showOosProducts = ui->OOSCheckBox->isChecked();
    local.animationsEnabled = ui->animationsCheckBox->isChecked();
    local.notificationSound = ui->notifSoundCheckBox->isChecked();
    local.save();

    StyleManager::applyTheme(this, color, font);
    updateIcons();
}

// ============================================================================
// APP SETTINGS — BACKGROUND COLOR (light theme tint)
// ============================================================================

void SettingsWindow::onBgColorClicked()
{
    const QColor current(LocalSettings::load().backgroundColor);
    const QColor chosen = QColorDialog::getColor(
        current.isValid() ? current : QColor("#E8D8BC"),
        this, tr("Pick a background color"));
    if (chosen.isValid())
        applyBackgroundColor(chosen);
}

// Re-apply the current theme to this window — installs the background image
// (if any) through StyleManager; otherwise the gradient / color tint shows.
void SettingsWindow::reapplyThemeToSelf()
{
    const LocalSettings local = LocalSettings::load();
    const StyleManager::ColorScheme cs = (local.colorScheme == "light")
        ? StyleManager::ColorScheme::Light : StyleManager::ColorScheme::Dark;
    StyleManager::FontSize fs;
    if (local.fontSize == "large")       fs = StyleManager::FontSize::Large;
    else if (local.fontSize == "small")  fs = StyleManager::FontSize::Small;
    else                                 fs = StyleManager::FontSize::Medium;
    StyleManager::applyTheme(this, cs, fs);
}

void SettingsWindow::applyBackgroundColor(const QColor& color)
{
    LocalSettings local = LocalSettings::load();
    local.backgroundColor = color.isValid() ? color.name() : QString();
    local.save();

    // Re-apply the theme so the new background tint takes effect immediately.
    reapplyThemeToSelf();

    updateBgSwatch(local.backgroundColor);
}

void SettingsWindow::updateBgSwatch(const QString& hex)
{
    const QColor c(hex);
    const QString fill = c.isValid() ? c.name() : QStringLiteral("#E8D8BC");
    ui->bgColorBtn->setStyleSheet(
        QStringLiteral("background-color: %1; border: 1px solid rgba(0,0,0,0.25);"
                       " min-width: 28px; max-width: 28px; min-height: 28px; max-height: 28px;"
                       " padding: 0; border-radius: 14px;").arg(fill));
}

// ============================================================================
// APP SETTINGS — BACKGROUND IMAGE (copied into app data, scaled to cover)
// ============================================================================

void SettingsWindow::onBgImageChooseClicked()
{
    const QString file = QFileDialog::getOpenFileName(
        this, tr("Choose background image"), QString(),
        tr("Images (*.png *.jpg *.jpeg *.bmp *.webp);;All files (*)"));
    if (file.isEmpty())
        return;

    // Copy the file into app data so the background survives the source being
    // moved or deleted, and so every app reads the same stable path.
    const QFileInfo fi(file);
    const QString ext = fi.suffix().isEmpty() ? QStringLiteral("png")
                                              : fi.suffix().toLower();
    const QString dest = POSConfig::getAppDataPath() + "background." + ext;

    // Drop stale copies from other formats first (QFile::copy refuses an
    // existing destination).
    for (const char* stale : {".png", ".jpg", ".jpeg", ".bmp", ".webp"}) {
        const QString p = POSConfig::getAppDataPath() + "background" + stale;
        if (p != dest && QFile::exists(p))
            QFile::remove(p);
    }
    if (QFile::exists(dest))
        QFile::remove(dest);
    if (!QFile::copy(file, dest)) {
        AppHelpers::showWarning(this, tr("Could not copy the selected image into the app data folder."), tr("Background image"));
        return;
    }
    // A copy keeps the source's permissions; a read-only file could not be
    // replaced or removed later on Windows.
    QFile::setPermissions(dest, QFile::permissions(dest) | QFile::WriteOwner);

    LocalSettings local = LocalSettings::load();
    local.backgroundImage = dest;
    local.save();

    reapplyThemeToSelf();      // installs the scaled image behind this window
    updateBgImagePreview();
}

void SettingsWindow::onBgImageRemoveClicked()
{
    LocalSettings local = LocalSettings::load();
    const QString old = local.backgroundImage;
    local.backgroundImage.clear();
    local.save();
    if (!old.isEmpty())
        QFile::remove(old);    // clean up the copy we made

    reapplyThemeToSelf();      // uninstalls the image -> color fallback shows
    updateBgImagePreview();
}

// Save the fill mode + dim scrim from the widgets and re-apply live.
void SettingsWindow::applyBackgroundImageOptions()
{
    LocalSettings local = LocalSettings::load();
    const char* keys[] = {"cover", "stretch", "fit"};
    local.backgroundScale = keys[qBound(0, ui->bgScaleSelect->currentIndex(), 2)];
    local.backgroundDim = qBound(0, ui->bgDimSlider->value(), 100);
    local.save();

    ui->bgDimValue->setText(QString("%1%").arg(local.backgroundDim));
    reapplyThemeToSelf();
}

void SettingsWindow::updateBgImagePreview()
{
    const QPixmap pix(LocalSettings::load().backgroundImage);
    if (pix.isNull()) {
        ui->bgImagePreview->setPixmap(QPixmap());
        ui->bgImagePreview->setText(tr("None"));
        ui->bgImagePreview->setStyleSheet(
            QStringLiteral("color: #8a8a8a; font-size: 10px;"
                           " border: 1px dashed rgba(0,0,0,0.3);"
                           " border-radius: 6px;"));
    } else {
        ui->bgImagePreview->setText(QString());
        ui->bgImagePreview->setStyleSheet(
            QStringLiteral("border: 1px solid rgba(0,0,0,0.3);"
                           " border-radius: 6px;"));
        // Fixed thumb size: the label may not be laid out yet when this runs.
        ui->bgImagePreview->setPixmap(pix.scaled(QSize(56, 30),
                                                 Qt::KeepAspectRatio,
                                                 Qt::SmoothTransformation));
    }
}

// ============================================================================
// REGION — COUNTRY & CURRENCY
// ============================================================================

void SettingsWindow::populateCountryCurrencyCombos()
{
    QSignalBlocker blockCountry(m_countrySelect);
    QSignalBlocker blockCurrency(m_currencySelect);

    m_countrySelect->clear();
    const auto& countries = CountryData::countries();
    for (const auto& c : countries)
        m_countrySelect->addItem(c.name + " (" + c.code + ")", c.code);

    // Each currency once.
    m_currencySelect->clear();
    QSet<QString> seen;
    for (const auto& c : countries) {
        if (!seen.contains(c.currency)) {
            seen.insert(c.currency);
            m_currencySelect->addItem(c.currency, c.currency);
        }
    }
}

void SettingsWindow::loadCountryCurrency()
{
    QSignalBlocker blockCountry(m_countrySelect);
    QSignalBlocker blockCurrency(m_currencySelect);

    QSqlQuery query(LocalDb::db());
    QString countryCode = "TN";
    QString currencyCode = "TND";
    if (query.exec("SELECT country, currency FROM app_settings WHERE id = 1") && query.next()) {
        if (!query.value(0).toString().trimmed().isEmpty())
            countryCode = query.value(0).toString().trimmed();
        if (!query.value(1).toString().trimmed().isEmpty())
            currencyCode = query.value(1).toString().trimmed();
    }
    m_currentCountry = CountryData::findByCode(countryCode);

    const int countryIdx = m_countrySelect->findData(countryCode);
    if (countryIdx >= 0) m_countrySelect->setCurrentIndex(countryIdx);
    const int currencyIdx = m_currencySelect->findData(currencyCode);
    if (currencyIdx >= 0) m_currencySelect->setCurrentIndex(currencyIdx);

    updateRegionalFields();
    // The profile's phone number was split with the default prefix: reload it.
    loadProfile();
}

void SettingsWindow::saveCountryCurrency()
{
    const QString countryCode = m_countrySelect->currentData().toString();
    const QString currencyCode = m_currencySelect->currentData().toString();

    QSqlQuery query(LocalDb::db());
    query.prepare("UPDATE app_settings SET country = ?, currency = ? WHERE id = 1");
    query.addBindValue(countryCode.isEmpty() ? QVariant() : QVariant(countryCode));
    query.addBindValue(currencyCode.isEmpty() ? QVariant() : QVariant(currencyCode));
    if (!query.exec())
        qWarning() << "Failed to save country/currency:" << query.lastError().text();

    // Prices in this window follow the new currency at once; the other apps
    // pick it up the next time they open.
    AppHelpers::loadCurrencyConfig(LocalDb::db());
}

void SettingsWindow::onCountryChanged(int index)
{
    if (index < 0) return;

    m_currentCountry = CountryData::findByCode(m_countrySelect->currentData().toString());

    // The country's usual currency, without saving twice.
    {
        QSignalBlocker block(m_currencySelect);
        const int curIdx = m_currencySelect->findData(m_currentCountry.currency);
        if (curIdx >= 0) m_currencySelect->setCurrentIndex(curIdx);
    }

    updateRegionalFields();
    onPhoneChanged(ui->phoneField->text());
    onPostalChanged(ui->postalField->text());
    saveCountryCurrency();
}

void SettingsWindow::onCurrencyChanged(int index)
{
    if (index < 0) return;
    saveCountryCurrency();
}

void SettingsWindow::updateRegionalFields()
{
    m_phonePrefix->setText(m_currentCountry.phoneCode);

    ui->phoneField->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QString("^\\d{0,%1}$").arg(m_currentCountry.phoneDigits)), this));
    ui->phoneField->setMaxLength(m_currentCountry.phoneDigits);
    ui->phoneField->setPlaceholderText(tr("%1 digits").arg(m_currentCountry.phoneDigits));

    if (m_currentCountry.postalDigits > 0) {
        ui->postalField->setValidator(new QRegularExpressionValidator(
            QRegularExpression(QString("^\\d{0,%1}$").arg(m_currentCountry.postalDigits)), this));
        ui->postalField->setMaxLength(m_currentCountry.postalDigits);
        ui->postalField->setPlaceholderText(tr("%1 digits").arg(m_currentCountry.postalDigits));
    } else {
        ui->postalField->setValidator(nullptr);
        ui->postalField->setMaxLength(10);
        ui->postalField->setPlaceholderText(tr("Postal code"));
    }
}

// ============================================================================
// LANGUAGE
// ============================================================================

void SettingsWindow::populateLanguageCombo()
{
    QSignalBlocker block(m_languageSelect);
    m_languageSelect->clear();
    for (const auto& lang : TranslationManager::instance().availableLanguages())
        m_languageSelect->addItem(lang.name, lang.code);
}

void SettingsWindow::loadLanguage()
{
    QSignalBlocker block(m_languageSelect);
    const int idx = m_languageSelect->findData(TranslationManager::instance().currentLanguage());
    if (idx >= 0)
        m_languageSelect->setCurrentIndex(idx);
}

void SettingsWindow::onLanguageChanged(int index)
{
    if (index < 0) return;
    const QString langCode = m_languageSelect->currentData().toString();
    if (langCode == TranslationManager::instance().currentLanguage())
        return;
    TranslationManager::instance().loadLanguage(langCode);

    // Every window is built in the language it opens with: reopen Settings
    // so all of it switches, and let the user know the other apps follow
    // the next time they open.
    AppHelpers::showInfo(this,
        tr("The language has been changed.\n\nSettings will reopen now. Other POSBox apps use the new "
           "language the next time they open."),
        tr("Language"));
    const QStringList args = QCoreApplication::arguments().mid(1);
    if (QProcess::startDetached(QCoreApplication::applicationFilePath(), args,
                                QCoreApplication::applicationDirPath())) {
        // The new instance must not see this one as already running.
        AppLock::clear("settings.lock");
        QCoreApplication::quit();
    }
}

// ============================================================================
// BACKUP & RESTORE (local, manual)
// ============================================================================
// A backup is one zip of the whole app-data folder (database, logo, product
// pictures, background image) written to the chosen folder. Restore puts a
// backup's files back and closes every POSBox app.

void SettingsWindow::loadSyncPath()
{
    QSqlQuery query(LocalDb::db());
    if (query.exec("SELECT sync_path, last_backup FROM app_settings WHERE id = 1") && query.next()) {
        m_syncPathEdit->setText(query.value(0).toString());
        const QString lastBackup = query.value(1).toString();
        m_lastBackupLabel->setText(lastBackup.isEmpty() ? tr("Last backup: Never")
                                                        : tr("Last backup: %1").arg(lastBackup));
    } else {
        m_lastBackupLabel->setText(tr("Last backup: Never"));
    }
}

void SettingsWindow::saveSyncPath(const QString& path)
{
    QSqlQuery query(LocalDb::db());
    query.prepare("UPDATE app_settings SET sync_path = ? WHERE id = 1");
    query.addBindValue(path);
    query.exec();
}

void SettingsWindow::onSyncPathBrowseClicked()
{
    const QString current = m_syncPathEdit->text().trimmed();
    const QString dir = QFileDialog::getExistingDirectory(
        this, tr("Select Backup Folder"), current.isEmpty() ? QDir::homePath() : current);
    if (dir.isEmpty()) return;

    m_syncPathEdit->setText(dir);
    saveSyncPath(dir);
}

QString SettingsWindow::zipToolPath() const
{
#ifdef Q_OS_WIN
    const QStringList paths = {
        "C:/Program Files/7-Zip/7z.exe",
        "C:/Program Files (x86)/7-Zip/7z.exe",
    };
    for (const QString& p : paths) {
        if (QFile::exists(p)) return p;
    }
    return "7z";
#else
    return "zip";
#endif
}

bool SettingsWindow::runZipProcess(const QStringList& args, const QString& tool, QString* errorOut,
                                   const QString& workingDir)
{
    QProcess process;
    if (!workingDir.isEmpty())
        process.setWorkingDirectory(workingDir);
    process.start(tool, args);

    if (!process.waitForStarted(5000)) {
        if (errorOut) *errorOut = tr("Could not start %1. Is it installed?").arg(tool);
        return false;
    }
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const bool finished = process.waitForFinished(120000);
    QApplication::restoreOverrideCursor();
    if (!finished) {
        if (errorOut) *errorOut = tr("The operation took too long.");
        process.kill();
        return false;
    }
    if (process.exitCode() != 0) {
        const QString err = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
        if (errorOut) *errorOut = err.isEmpty() ? tr("Exited with code %1").arg(process.exitCode()) : err;
        return false;
    }
    return true;
}

bool SettingsWindow::validateBackupZip(const QString& zipPath)
{
    QProcess process;
#ifdef Q_OS_WIN
    process.start(zipToolPath(), {"l", zipPath});
#else
    process.start("unzip", {"-l", zipPath});
#endif
    if (!process.waitForStarted(5000) || !process.waitForFinished(15000))
        return false;

    // A POSBox backup always holds the database.
    const QString output = QString::fromLocal8Bit(process.readAllStandardOutput());
    return output.contains(SettingsConstants::BACKUP_MARKER_DB);
}

void SettingsWindow::onBackupClicked()
{
    const QString syncPath = m_syncPathEdit->text().trimmed();
    if (syncPath.isEmpty()) {
        AppHelpers::showWarning(this, AppHelpers::trMsg(SettingsConstants::MSG_BACKUP_PATH_MISSING), tr("No Backup Folder"));
        return;
    }
    if (!QDir(syncPath).exists()) {
        AppHelpers::showWarning(this, tr("The backup folder does not exist. Please select a valid folder."),
                                tr("Folder Not Found"));
        return;
    }

    const QString dataPath  = POSConfig::getAppDataPath();
    const QString outputZip = syncPath + "/" + SettingsConstants::BACKUP_FILENAME;

    // Always exactly one backup file in the folder.
    if (QFile::exists(outputZip))
        QFile::remove(outputZip);

    QString errMsg;
#ifdef Q_OS_WIN
    const QStringList args = {
        "a", "-tzip",
        QDir::toNativeSeparators(outputZip),
        QDir::toNativeSeparators(dataPath + "*")
    };
    if (!runZipProcess(args, zipToolPath(), &errMsg)) {
        AppHelpers::showError(this,
            tr("Failed to create the backup archive (7-Zip).\n\n%1\n\nMake sure 7-Zip is installed.").arg(errMsg),
            tr("Backup Failed"));
        return;
    }
#else
    if (!runZipProcess({"-r", outputZip, "."}, "zip", &errMsg, dataPath)) {
        AppHelpers::showError(this, AppHelpers::trMsg(SettingsConstants::MSG_BACKUP_ZIP_FAILED).arg("zip") + "\n\n" + errMsg,
                              tr("Backup Failed"));
        return;
    }
#endif

    const QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm");
    QSqlQuery query(LocalDb::db());
    query.prepare("UPDATE app_settings SET last_backup = ? WHERE id = 1");
    query.addBindValue(timestamp);
    query.exec();

    m_lastBackupLabel->setText(tr("Last backup: %1").arg(timestamp));
    AppHelpers::showInfo(this, AppHelpers::trMsg(SettingsConstants::MSG_BACKUP_SUCCESS), tr("Backup Complete"));
}

void SettingsWindow::onImportClicked()
{
    const QString start = m_syncPathEdit->text().trimmed();
    const QString zipPath = QFileDialog::getOpenFileName(
        this, tr("Select POSBox Backup"), start.isEmpty() ? QDir::homePath() : start,
        tr("POSBox Backup (*.zip)"));
    if (zipPath.isEmpty()) return;

    if (!validateBackupZip(zipPath)) {
        AppHelpers::showWarning(this, AppHelpers::trMsg(SettingsConstants::MSG_IMPORT_INVALID_ZIP), tr("Invalid Backup"));
        return;
    }

    if (!AppHelpers::confirmAction(this, AppHelpers::trMsg(SettingsConstants::MSG_IMPORT_CONFIRM), tr("Restore Backup")))
        return;

    // The database is replaced underneath: release this app's connection.
    const QString dataPath = POSConfig::getAppDataPath();
    QString errMsg;
#ifdef Q_OS_WIN
    const QStringList args = {
        "x", QDir::toNativeSeparators(zipPath),
        "-o" + QDir::toNativeSeparators(dataPath), "-y"
    };
    const bool ok = runZipProcess(args, zipToolPath(), &errMsg);
#else
    const bool ok = runZipProcess({"-o", zipPath, "-d", dataPath}, "unzip", &errMsg);
#endif

    if (!ok) {
        AppHelpers::showError(this, AppHelpers::trMsg(SettingsConstants::MSG_IMPORT_FAILED) + "\n\n" + errMsg,
                              tr("Import Failed"));
        return;
    }

    AppHelpers::showInfo(this, AppHelpers::trMsg(SettingsConstants::MSG_IMPORT_SUCCESS), tr("Import Successful"));
    // Every POSBox app still holds the old data: close them all.
    ChangeNotifier notifier;
    notifier.notifyChange(ChangeType::SESSION_LOGOUT_REQUESTED, 0);
    QApplication::quit();
}

// ============================================================================
// NOTIFICATIONS SETTINGS
// ============================================================================

void SettingsWindow::loadNotificationSettings()
{
    QSignalBlocker blocker1(ui->openTicketCheck);
    QSignalBlocker blocker2(ui->openTicketDays);
    QSignalBlocker blocker3(ui->outStockCheck);
    QSignalBlocker blocker4(ui->nearOutStockCheck);
    QSignalBlocker blocker5(ui->lowStockMinSpin);

    QList<DataRow> rows = m_api->select("notification_settings", {{"id", 1}});
    if (!m_api->ok() || rows.isEmpty())
        return;
    DataRow row = rows.first();

    ui->openTicketCheck->setChecked(row.value("open_ticket_enabled").toInt() != 0);
    ui->openTicketDays->setCurrentIndex(ui->openTicketDays->findData(row.value("open_ticket_days").toInt()));
    ui->outStockCheck->setChecked(row.value("out_of_stock_enabled").toInt() != 0);
    ui->nearOutStockCheck->setChecked(row.value("nearly_out_of_stock_enabled").toInt() != 0);
    ui->lowStockMinSpin->setValue(row.value("low_stock_min").toInt());
}

void SettingsWindow::saveNotificationSettings()
{
    QVariantMap values;
    values["open_ticket_enabled"] = ui->openTicketCheck->isChecked() ? 1 : 0;
    values["open_ticket_days"] = ui->openTicketDays->currentData().toInt();
    values["out_of_stock_enabled"] = ui->outStockCheck->isChecked() ? 1 : 0;
    values["nearly_out_of_stock_enabled"] = ui->nearOutStockCheck->isChecked() ? 1 : 0;
    values["low_stock_min"] = ui->lowStockMinSpin->value();
    if (!m_api->update("notification_settings", values, {{"id", 1}}))
        qWarning() << "Failed to save notification settings:" << m_api->lastError();
}
