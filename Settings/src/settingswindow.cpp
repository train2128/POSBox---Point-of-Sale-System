#include "settingswindow.h"
#include "ui_settingswindow.h"
#include "SettingsConstants.h"
#include "PrinterConstants.h"
#include "StyleManager.h"
#include "UserDialog.h"
#include "UserSession.h"
#include "SessionManager.h"
#include "DatabaseManager.h"
#include "QueryRepository.h"
#include "CommonConfig.h"
#include "ThermalPrinter.h"
#include "NotificationSettings.h"
#include "PermissionManager.h"
#include "AccessControl.h"
#include "LogoutManager.h"
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
#include <QSqlQuery>
#include <QSqlError>
#include <QMessageBox>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSerialPortInfo>
#include <QProcess>
#include <QDateTime>
#include <QFormLayout>

SettingsWindow::SettingsWindow(UserSession* session, DatabaseManager* db, QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::SettingsWindow)
    , m_session(session)
    , m_db(db)
    , m_permissionManager(nullptr)
    , m_usersModel(nullptr)
{
    ui->setupUi(this);
    m_permissionManager = new PermissionManager(m_db->database());
    setupUI();
    setupValidators();
    setupConnections();
    loadProfile();
    loadUsers();
    loadAppSettings();
    loadSyncPath();
    populatePrinterCombos();
    scanComPorts();
    loadPrinterSettings();
    loadNotificationSettings();
}

SettingsWindow::~SettingsWindow()
{
    delete m_permissionManager;
    delete ui;
}

// ============================================================================
// SETUP
// ============================================================================

void SettingsWindow::setupUI()
{
    setWindowTitle("POSBox - Settings");

    if (centralWidget()) {
        centralWidget()->setObjectName("centralwidget");
    }

    // Always open on the first tab
    ui->tabWidget->setCurrentIndex(0);

    // Tab titles
    ui->tabWidget->setTabText(0, "Shop Profile");
    ui->tabWidget->setTabText(1, "Users");
    ui->tabWidget->setTabText(2, "App Settings");
    ui->tabWidget->setTabText(3, "Printer");
    ui->tabWidget->setTabText(4, "Notifications");

    // userPermissions titles
    ui->themeSettings->setTitle("Theme");
    ui->databaseSettings->setTitle("Database");

    // Button labels
    ui->saveSettingsBtn->setText("Save");
    ui->cancelSettingsBtn->setText("Cancel");
    ui->logoutBtn->setText("Logout");
    ui->userAddBtn->setText("Add");
    ui->userEditBtn->setText("Edit");
    ui->userDeleteBtn->setText("Delete");
    ui->syncPathBtn->setText("Browse");
    ui->backupBtn->setText("Backup");
    ui->restoreBtn->setText("Import");
    ui->portRefreshBtn->setText("Refresh");
    ui->savePrinterBtn->setText("Save");
    ui->printTestBtn->setText("Test Print");

    // Label texts
    ui->shopNameLabel->setText("Shop Name");
    ui->taxLabel->setText("Tax ID");
    ui->adressLable->setText("Address");
    ui->phoneLabel->setText("Phone");
    ui->emailLabel->setText("Email");
    ui->websiteLabel->setText("Website");
    ui->facebookLabel->setText("Facebook");
    ui->instagramLabel->setText("Instagram");
    ui->tiktokLabel->setText("TikTok");
    ui->whatsappLabel->setText("WhatsApp");
    ui->baudRateLabel->setText("Baud Rate");
    ui->dataBitsLabel->setText("Data Bits");
    ui->parityLabel->setText("Parity");
    ui->stopBitsLabel->setText("Stop Bits");
    ui->flowLabel->setText("Flow Control");
    ui->latencyLabel->setText("Latency");
    ui->lastSyncLabel->setText("Last sync: Never");
    ui->printerStatusLabel->setText("Status: Not connected");

    // ── Notifications tab ─────────────────────────────────────────────────────
    ui->openTicketLabel->setText("Open Tickets (days before + frequency):");
    ui->outStockLabel->setText("Out of Stock:");
    ui->nearOutStockLabel->setText("Nearly Out of Stock:");

    // Populate day combos (1-30)
    for (int i = 1; i <= 30; ++i) {
        ui->openTicketDays->addItem(QString::number(i), i);
        ui->openTicketFrequancy->addItem(QString::number(i), i);
    }

    // Placeholder texts
    ui->shopNameField->setPlaceholderText("Shop name");
    ui->taxField->setPlaceholderText("Tax ID");
    ui->adressField->setPlaceholderText("Address");
    ui->cityField->setPlaceholderText("City");
    ui->postalField->setPlaceholderText(QString("%1 digits").arg(SettingsConstants::POSTAL_DIGIT_COUNT));
    ui->emailField->setPlaceholderText("example@domain.com");
    ui->websiteField->setPlaceholderText("https://");
    ui->facebookField->setPlaceholderText("Facebook URL");
    ui->instagramField->setPlaceholderText("Instagram URL");
    ui->tiktokField->setPlaceholderText("TikTok URL");
    ui->whatsappField->setPlaceholderText("WhatsApp number");
    ui->syncPathPath->setPlaceholderText("Sync folder path");
    ui->syncPathPath->setReadOnly(true);

    // ── Register settings ─────────────────────────────────────────────────────
    ui->registerSettings->setTitle("Register");
    ui->OOSLabel->setText("Show out of stock items in Register:");
    ui->OOSCheckBox->setText("");

    // ── Printer enable ────────────────────────────────────────────────────────
    ui->printerEnableLabel->setText("Enable Thermal Printer:");
    ui->printerEnableBox->setText("");

    // Center text in all profile fields
    ui->shopNameField->setAlignment(Qt::AlignCenter);
    ui->taxField->setAlignment(Qt::AlignCenter);
    ui->adressField->setAlignment(Qt::AlignCenter);
    ui->cityField->setAlignment(Qt::AlignCenter);
    ui->postalField->setAlignment(Qt::AlignCenter);
    ui->emailField->setAlignment(Qt::AlignCenter);
    ui->websiteField->setAlignment(Qt::AlignCenter);
    ui->facebookField->setAlignment(Qt::AlignCenter);
    ui->instagramField->setAlignment(Qt::AlignCenter);
    ui->tiktokField->setAlignment(Qt::AlignCenter);
    ui->whatsappField->setAlignment(Qt::AlignCenter);

    ui->phoneField->setAlignment(Qt::AlignCenter);
    ui->phoneField->setPlaceholderText(QString("%1 digits").arg(SettingsConstants::PHONE_DIGIT_COUNT));

    // Logo label
    ui->logoImageLabel->setFixedSize(SettingsConstants::LOGO_IMAGE_SIZE,
                                     SettingsConstants::LOGO_IMAGE_SIZE);
    ui->logoImageLabel->setCursor(QCursor(Qt::PointingHandCursor));
    ui->logoImageLabel->setAlignment(Qt::AlignCenter);
    ui->logoImageLabel->setText("Click to\nupload logo");
    ui->logoImageLabel->setStyleSheet(
        "QLabel {"
        "  background-color: #3d3d3d;"
        "  border: 2px dashed #606060;"
        "  border-radius: 8px;"
        "  color: #888888;"
        "  font-size: 11px;"
        "  font-weight: bold;"
        "}"
        "QLabel:hover {"
        "  background-color: #454545;"
        "  border-color: #4A90E2;"
        "}"
        );

    if (QVBoxLayout* profileLayout = qobject_cast<QVBoxLayout*>(
            ui->logoImageLabel->parentWidget()->layout())) {
        int idx = profileLayout->indexOf(ui->logoImageLabel);
        if (idx >= 0) {
            profileLayout->removeWidget(ui->logoImageLabel);

            QWidget* logoRow = new QWidget(this);
            QHBoxLayout* logoLayout = new QHBoxLayout(logoRow);
            logoLayout->setContentsMargins(0, 4, 0, 4);
            logoLayout->addStretch();
            logoLayout->addWidget(ui->logoImageLabel);
            logoLayout->addStretch();

            profileLayout->insertWidget(idx, logoRow);
        }
    }

    // ---- Users tab ----
    m_usersModel = new QStandardItemModel(0, 5, this);
    m_usersModel->setHorizontalHeaderLabels({
        "Username", "Display Name", "Role", "Status", "Last Login"
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
    ui->userPermissions->setTitle("Permissions");
    ui->pWithdrawLabel->setText("Withdraw Cash:");
    ui->pDeleteTicketLabel->setText("Delete Ticket:");
    ui->pDeleteProductLabel->setText("Delete Products:");
    ui->pAddCatLabel->setText("Add Categories:");
    ui->pDiscountsLabel->setText("Manage Discounts:");
    ui->pExpensesLabel->setText("Edit Expenses:");
    ui->pRecurringLabel->setText("Pay Recurring:");
    ui->permissionsSaveBtn->setText("Save Permissions");

    ui->userPermissions->setVisible(false);

    // Store checkboxes in a map for easy access
    // Disable Edit/Delete buttons initially
    ui->userEditBtn->setEnabled(false);
    ui->userDeleteBtn->setEnabled(false);

    // App Settings combos
    ui->colorSelect->addItems({"Dark", "Light"});
    ui->fontSelect->addItems({"Small", "Medium", "Large"});
    ui->fontSelect->setCurrentIndex(1);
    ui->bgSelect->addItems({"Default", "Caramel", "Black", "Blue"});

    updateUserDisplay();
    showMaximized();
}

void SettingsWindow::setupValidators()
{
    ui->postalField->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QString("^\\d{0,%1}$").arg(SettingsConstants::POSTAL_DIGIT_COUNT)), this));
    ui->postalField->setMaxLength(SettingsConstants::POSTAL_DIGIT_COUNT);

    ui->phoneField->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QString("^\\d{0,%1}$").arg(SettingsConstants::PHONE_DIGIT_COUNT)), this));
    ui->phoneField->setMaxLength(SettingsConstants::PHONE_DIGIT_COUNT);
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
    connect(ui->taxField, &QLineEdit::textChanged, this, &SettingsWindow::onTaxIdChanged);

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

    // App Settings — auto-save + live-apply on color, font, or background change
    connect(ui->colorSelect, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { saveAppSettings(); });
    connect(ui->fontSelect,  QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { saveAppSettings(); });
    connect(ui->bgSelect,    QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { saveAppSettings(); });

    // Database
    connect(ui->syncPathBtn, &QPushButton::clicked, this, &SettingsWindow::onSyncPathBrowseClicked);
    connect(ui->backupBtn,   &QPushButton::clicked, this, &SettingsWindow::onBackupClicked);
    connect(ui->restoreBtn,  &QPushButton::clicked, this, &SettingsWindow::onImportClicked);

    // OOS checkbox — auto-save immediately on toggle
    connect(ui->OOSCheckBox, &QCheckBox::toggled, this, [this](bool) { saveAppSettings(); });

    // Printer enable checkbox
    connect(ui->printerEnableBox, &QCheckBox::toggled,
            this, &SettingsWindow::onPrinterEnableToggled);

    // Notifications — auto-save on any change
    connect(ui->openTicketCheck, &QCheckBox::toggled, this, [this](bool) { saveNotificationSettings(); });
    connect(ui->openTicketDays, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { saveNotificationSettings(); });
    connect(ui->openTicketFrequancy, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { saveNotificationSettings(); });
    connect(ui->outStockCheck, &QCheckBox::toggled, this, [this](bool) { saveNotificationSettings(); });
    connect(ui->nearOutStockCheck, &QCheckBox::toggled, this, [this](bool) { saveNotificationSettings(); });
}

void SettingsWindow::updateUserDisplay()
{
    if (m_session) {
        ui->userLabel->setText(QString("%1  |  %2")
                                   .arg(m_session->displayName())
                                   .arg(m_session->role()));
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
    QSqlQuery query(m_db->database());
    query.prepare(Queries::PROFILE_SELECT);

    if (!query.exec() || !query.next()) {
        qWarning() << "Failed to load shop profile:" << query.lastError().text();
        return;
    }

    ui->shopNameField->setText(query.value("shop_name").toString());
    ui->taxField->setText(query.value("tax_id").toString());
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
    QString prefix   = QString(SettingsConstants::PHONE_PREFIX);
    if (phoneRaw.startsWith(prefix)) {
        phoneRaw = phoneRaw.mid(prefix.length());
    }
    ui->phoneField->setText(phoneRaw);

    m_existingLogoFilename = query.value("logo_path").toString();
    m_newLogoSourcePath.clear();

    if (!m_existingLogoFilename.isEmpty()) {
        QString fullPath = POSConfig::getAppDataPath()
        + POSConfig::LOGO_IMAGE_FOLDER
            + "/" + m_existingLogoFilename;
        displayLogo(fullPath);
    } else {
        ui->logoImageLabel->setPixmap(QPixmap());
        ui->logoImageLabel->setText("Click to\nupload logo");
    }

    onPhoneChanged(ui->phoneField->text());
    onPostalChanged(ui->postalField->text());
    onEmailChanged(ui->emailField->text());
    onTaxIdChanged(ui->taxField->text());
}

bool SettingsWindow::saveProfile()
{
    if (!validateFields()) return false;

    QString logoFilenameToSave = m_existingLogoFilename;

    if (!m_newLogoSourcePath.isEmpty()) {
        QString destFolder = POSConfig::getAppDataPath()
        + POSConfig::LOGO_IMAGE_FOLDER;
        QDir().mkpath(destFolder);

        QString fileName = QFileInfo(m_newLogoSourcePath).fileName();
        QString destPath = destFolder + "/" + fileName;

        if (QFile::exists(destPath)) QFile::remove(destPath);

        if (QFile::copy(m_newLogoSourcePath, destPath)) {
            logoFilenameToSave     = fileName;
            m_existingLogoFilename = fileName;
            m_newLogoSourcePath.clear();
            qDebug() << "Logo copied to:" << destPath;
        } else {
            qWarning() << "Failed to copy logo from:" << m_newLogoSourcePath;
        }
    }

    QString fullPhone;
    if (!ui->phoneField->text().trimmed().isEmpty()) {
        fullPhone = QString(SettingsConstants::PHONE_PREFIX) + ui->phoneField->text().trimmed();
    }

    QSqlQuery query(m_db->database());
    query.prepare(Queries::PROFILE_UPDATE);
    query.addBindValue(ui->shopNameField->text().trimmed());
    query.addBindValue(ui->taxField->text().trimmed().isEmpty()
                           ? QVariant() : QVariant(ui->taxField->text().trimmed()));
    query.addBindValue(ui->adressField->text().trimmed().isEmpty()
                           ? QVariant() : QVariant(ui->adressField->text().trimmed()));
    query.addBindValue(ui->postalField->text().trimmed().isEmpty()
                           ? QVariant() : QVariant(ui->postalField->text().trimmed()));
    query.addBindValue(ui->cityField->text().trimmed().isEmpty()
                           ? QVariant() : QVariant(ui->cityField->text().trimmed()));
    query.addBindValue(QVariant());
    query.addBindValue(fullPhone.isEmpty() ? QVariant() : QVariant(fullPhone));
    query.addBindValue(ui->emailField->text().trimmed().isEmpty()
                           ? QVariant() : QVariant(ui->emailField->text().trimmed()));
    query.addBindValue(ui->websiteField->text().trimmed().isEmpty()
                           ? QVariant() : QVariant(ui->websiteField->text().trimmed()));
    query.addBindValue(ui->facebookField->text().trimmed().isEmpty()
                           ? QVariant() : QVariant(ui->facebookField->text().trimmed()));
    query.addBindValue(ui->instagramField->text().trimmed().isEmpty()
                           ? QVariant() : QVariant(ui->instagramField->text().trimmed()));
    query.addBindValue(ui->tiktokField->text().trimmed().isEmpty()
                           ? QVariant() : QVariant(ui->tiktokField->text().trimmed()));
    query.addBindValue(ui->whatsappField->text().trimmed().isEmpty()
                           ? QVariant() : QVariant(ui->whatsappField->text().trimmed()));
    query.addBindValue(QVariant());
    query.addBindValue(QVariant());
    query.addBindValue(QVariant());
    query.addBindValue(logoFilenameToSave.isEmpty()
                           ? QVariant() : QVariant(logoFilenameToSave));

    if (!query.exec()) {
        qWarning() << "Failed to save profile:" << query.lastError().text();
        QMessageBox::warning(this, "Save Failed", SettingsConstants::MSG_SAVE_FAILED);
        return false;
    }

    QMessageBox::information(this, "Saved", SettingsConstants::MSG_SAVE_SUCCESS);
    return true;
}

bool SettingsWindow::validateFields()
{
    if (ui->shopNameField->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Validation Error", SettingsConstants::MSG_SHOP_NAME_EMPTY);
        ui->shopNameField->setFocus();
        return false;
    }
    if (!ui->phoneField->text().trimmed().isEmpty() && !isPhoneValid()) {
        QMessageBox::warning(this, "Validation Error",
                             QString(SettingsConstants::MSG_PHONE_INVALID)
                                 .arg(SettingsConstants::PHONE_DIGIT_COUNT));
        ui->phoneField->setFocus();
        return false;
    }
    if (!ui->postalField->text().trimmed().isEmpty() && !isPostalValid()) {
        QMessageBox::warning(this, "Validation Error",
                             QString(SettingsConstants::MSG_POSTAL_INVALID)
                                 .arg(SettingsConstants::POSTAL_DIGIT_COUNT));
        ui->postalField->setFocus();
        return false;
    }
    if (!ui->emailField->text().trimmed().isEmpty() && !isEmailValid()) {
        QMessageBox::warning(this, "Validation Error", SettingsConstants::MSG_EMAIL_INVALID);
        ui->emailField->setFocus();
        return false;
    }
    return true;
}

bool SettingsWindow::isPhoneValid() const
{
    QRegularExpression rx(QString("^\\d{%1}$").arg(SettingsConstants::PHONE_DIGIT_COUNT));
    return rx.match(ui->phoneField->text().trimmed()).hasMatch();
}

bool SettingsWindow::isPostalValid() const
{
    QRegularExpression rx(QString("^\\d{%1}$").arg(SettingsConstants::POSTAL_DIGIT_COUNT));
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

void SettingsWindow::onTaxIdChanged(const QString& text)
{
    if (text.trimmed().isEmpty()) {
        clearFieldState(ui->taxField);
        return;
    }

    int len = text.trimmed().length();
    if (len < 10) {
        setFieldState(ui->taxField, false);
    } else if (len > 10) {
        ui->taxField->setText(text.trimmed().left(10));
    } else {
        setFieldState(ui->taxField, true);
    }
}

void SettingsWindow::onLogoClicked()
{
    QString filePath = QFileDialog::getOpenFileName(
        this, "Select Logo Image", QString(),
        "Images (*.png *.jpg *.jpeg *.bmp)");
    if (filePath.isEmpty()) return;
    m_newLogoSourcePath = filePath;
    displayLogo(filePath);
}

void SettingsWindow::displayLogo(const QString& fullPath)
{
    QPixmap pixmap(fullPath);
    if (pixmap.isNull()) { qWarning() << "Failed to load logo:" << fullPath; return; }
    ui->logoImageLabel->setPixmap(
        pixmap.scaled(ui->logoImageLabel->size(),
                      Qt::KeepAspectRatio, Qt::SmoothTransformation));
    ui->logoImageLabel->setText(QString());
}

void SettingsWindow::onSaveClicked()   { saveProfile(); }
void SettingsWindow::onCancelClicked() { loadProfile(); }

// ============================================================================
// USERS
// ============================================================================

void SettingsWindow::loadUsers()
{
    m_usersModel->removeRows(0, m_usersModel->rowCount());

    QSqlQuery query(m_db->database());
    query.prepare(Queries::ACCOUNT_SELECT_ALL);

    if (!query.exec()) {
        qWarning() << "Failed to load users:" << query.lastError().text();
        return;
    }

    while (query.next()) {
        QString username    = query.value("username").toString();
        QString displayName = query.value("display_name").toString();
        QString role        = query.value("role").toString();
        bool    isActive    = query.value("is_active").toBool();
        QString lastLogin   = query.value("last_login").toString();

        QList<QStandardItem*> row;

        auto makeItem = [&](const QString& text) -> QStandardItem* {
            QStandardItem* item = new QStandardItem(text);
            item->setTextAlignment(Qt::AlignCenter);
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
        row << makeItem(lastLogin.isEmpty() ? "Never" : lastLogin);

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
    UserDialog dialog(m_db, UserDialog::Mode::Add, QString(), this);
    if (dialog.exec() == QDialog::Accepted) {
        QMessageBox::information(this, "Success", SettingsConstants::MSG_USER_ADD_SUCCESS);
        loadUsers();
    }
}

void SettingsWindow::onUserEditClicked()
{
    QString username = selectedUsername();
    if (username.isEmpty()) {
        QMessageBox::warning(this, "No Selection", SettingsConstants::MSG_USER_SELECT);
        return;
    }

    UserDialog dialog(m_db, UserDialog::Mode::Edit, username, this);
    if (dialog.exec() == QDialog::Accepted) {
        QMessageBox::information(this, "Success", SettingsConstants::MSG_USER_EDIT_SUCCESS);
        loadUsers();
    }
}

void SettingsWindow::onUserDeleteClicked()
{
    QString username = selectedUsername();
    if (username.isEmpty()) {
        QMessageBox::warning(this, "No Selection", SettingsConstants::MSG_USER_SELECT);
        return;
    }

    if (username == m_session->username()) {
        QMessageBox::warning(this, "Not Allowed", SettingsConstants::MSG_USER_CANNOT_DELETE_SELF);
        return;
    }

    QSqlQuery countQuery(m_db->database());
    countQuery.prepare("SELECT COUNT(*) FROM accounts WHERE role = 'Admin'");
    if (countQuery.exec() && countQuery.next()) {
        int adminCount = countQuery.value(0).toInt();

        QSqlQuery roleQuery(m_db->database());
        roleQuery.prepare("SELECT role FROM accounts WHERE username = ?");
        roleQuery.addBindValue(username);
        if (roleQuery.exec() && roleQuery.next()) {
            QString role = roleQuery.value(0).toString();
            if (role == "Admin" && adminCount <= 1) {
                QMessageBox::warning(this, "Not Allowed",
                                     SettingsConstants::MSG_USER_CANNOT_DELETE_LAST_ADMIN);
                return;
            }
        }
    }

    int confirm = QMessageBox::question(this, "Confirm Delete",
                                        QString(SettingsConstants::MSG_USER_DELETE_CONFIRM),
                                        QMessageBox::Yes | QMessageBox::No);
    if (confirm != QMessageBox::Yes) return;

    QSqlQuery query(m_db->database());
    query.prepare(Queries::ACCOUNT_DELETE);
    query.addBindValue(username);

    if (!query.exec()) {
        qWarning() << "Failed to delete user:" << query.lastError().text();
        QMessageBox::warning(this, "Error", "Failed to delete user.");
        return;
    }

    QMessageBox::information(this, "Deleted", SettingsConstants::MSG_USER_DELETE_SUCCESS);
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

    QSqlQuery query(m_db->database());
    query.prepare("SELECT role FROM accounts WHERE username = ?");
    query.addBindValue(username);

    if (!query.exec() || !query.next()) {
        clearPermissionsPanel();
        return;
    }

    QString role = query.value(0).toString();

    if (AccessControl::isAdminRole(role)) {
        ui->userPermissions->setVisible(true);
        ui->userPermissions->setTitle("Permissions (Admin has all permissions)");
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
    ui->userPermissions->setTitle(QString("Permissions for %1 (%2)").arg(username).arg(role));
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
    QMap<QString, bool> permissions = m_permissionManager->loadUserPermissions(username);

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

    m_permissionManager->saveUserPermission(username, AccessControl::PERMISSION_WITHDRAW_CASH,
                                            ui->pWithdrawCheck->isChecked());
    m_permissionManager->saveUserPermission(username, AccessControl::PERMISSION_DELETE_TICKET,
                                            ui->pDeleteTicketCheck->isChecked());
    m_permissionManager->saveUserPermission(username, AccessControl::PERMISSION_DELETE_PRODUCT,
                                            ui->pDeleteProductCheck->isChecked());
    m_permissionManager->saveUserPermission(username, AccessControl::PERMISSION_ADD_CATEGORY,
                                            ui->paddCatCheck->isChecked());
    m_permissionManager->saveUserPermission(username, AccessControl::PERMISSION_MANAGE_DISCOUNTS,
                                            ui->pDiscountsCheck->isChecked());
}

void SettingsWindow::clearPermissionsPanel()
{
    ui->userPermissions->setVisible(false);
    ui->pWithdrawCheck->setChecked(false);
    ui->pDeleteTicketCheck->setChecked(false);
    ui->pDeleteProductCheck->setChecked(false);
    ui->paddCatCheck->setChecked(false);
    ui->pDiscountsCheck->setChecked(false);
    ui->pExpensesCheck->setChecked(false);
    ui->pRecurringCheck->setChecked(false);
}

void SettingsWindow::onPermissionsSaveClicked()
{
    QString username = selectedUsername();
    if (username.isEmpty()) {
        return;
    }

    saveUserPermissions(username);
    QMessageBox::information(this, "Success", "Permissions saved successfully.");
}

// ============================================================================
// LOGOUT
// ============================================================================

void SettingsWindow::onLogoutClicked()
{
    if (!LogoutManager::confirmAndLogout(this, m_session->sessionId(), m_session->username())) {
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
    ui->latencySelect->setSuffix(" ms");

    // Paper Size — all common thermal printer widths
    ui->paperSizeSelect->clear();
    ui->paperSizeSelect->addItems({"58mm", "76mm", "80mm", "82.5mm", "104mm", "112mm"});

    // Encoding
    ui->encodingSelect->clear();
    ui->encodingSelect->addItems({"UTF-8", "IBM850", "CP1252", "GB2312"});
}

void SettingsWindow::scanComPorts()
{
    const QString currentPort = ui->portSelect->currentText();

    ui->portSelect->clear();

    const QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : ports)
        ui->portSelect->addItem(info.portName());

    if (ui->portSelect->count() == 0) {
        ui->printerStatusLabel->setText(SettingsConstants::MSG_PRINTER_NO_PORTS_FOUND);
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
    ui->encodingSelect->setCurrentText(PrinterConstants::DEFAULT_ENCODING);

    {
        QSignalBlocker block(ui->printerEnableBox);
        ui->printerEnableBox->setChecked(PrinterConstants::DEFAULT_ENABLED);
    }
    setPrinterControlsEnabled(PrinterConstants::DEFAULT_ENABLED);

    QSqlQuery query(m_db->database());
    query.exec("SELECT enabled, port, baud_rate, data_bits, parity, stop_bits, flow_control, latency_ms, paper_size, encoding FROM printer_settings WHERE id = 1");
    if (!query.next())
        return;

    const bool enabled = query.value(0).toInt() != 0;
    {
        QSignalBlocker block(ui->printerEnableBox);
        ui->printerEnableBox->setChecked(enabled);
    }
    setPrinterControlsEnabled(enabled);

    const QString savedPort = query.value(1).toString();
    const int portIdx = ui->portSelect->findText(savedPort);
    if (portIdx >= 0)
        ui->portSelect->setCurrentIndex(portIdx);

    const int savedBaud = query.value(2).toInt();
    const int baudIdx = ui->baudRateSelect->findData(savedBaud);
    if (baudIdx >= 0) ui->baudRateSelect->setCurrentIndex(baudIdx);

    const int savedBits = query.value(3).toInt();
    const int bitsIdx = ui->dataBitsSelect->findData(savedBits);
    if (bitsIdx >= 0) ui->dataBitsSelect->setCurrentIndex(bitsIdx);

    ui->paritySelect->setCurrentText(query.value(4).toString());
    ui->stopBitsSelect->setCurrentText(query.value(5).toString());
    ui->flowSelect->setCurrentText(query.value(6).toString());
    ui->latencySelect->setValue(query.value(7).toInt());
    ui->paperSizeSelect->setCurrentText(query.value(8).toString());
    ui->encodingSelect->setCurrentText(query.value(9).toString());
}

void SettingsWindow::savePrinterSettings()
{
    QSqlQuery query(m_db->database());
    query.prepare(R"(
        UPDATE printer_settings SET
            enabled = :enabled,
            port = :port,
            baud_rate = :baud,
            data_bits = :bits,
            parity = :parity,
            stop_bits = :stop,
            flow_control = :flow,
            latency_ms = :latency,
            paper_size = :paper,
            encoding = :encoding
        WHERE id = 1
    )");
    query.bindValue(":enabled", ui->printerEnableBox->isChecked() ? 1 : 0);
    query.bindValue(":port", ui->portSelect->currentText());
    query.bindValue(":baud", ui->baudRateSelect->currentData().toInt());
    query.bindValue(":bits", ui->dataBitsSelect->currentData().toInt());
    query.bindValue(":parity", ui->paritySelect->currentText());
    query.bindValue(":stop", ui->stopBitsSelect->currentText());
    query.bindValue(":flow", ui->flowSelect->currentText());
    query.bindValue(":latency", ui->latencySelect->value());
    query.bindValue(":paper", ui->paperSizeSelect->currentText());
    query.bindValue(":encoding", ui->encodingSelect->currentText());

    if (query.exec()) {
        ui->printerStatusLabel->setText(SettingsConstants::MSG_PRINTER_SAVE_SUCCESS);
    } else {
        ui->printerStatusLabel->setText(SettingsConstants::MSG_PRINTER_SAVE_FAILED);
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
        ui->printerStatusLabel->setText(SettingsConstants::MSG_PRINTER_DISABLED);
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
        ui->printerStatusLabel->setText(SettingsConstants::MSG_PRINTER_DISABLED);
        return;
    }

    if (ui->portSelect->currentText().isEmpty()) {
        ui->printerStatusLabel->setText(SettingsConstants::MSG_PRINTER_NO_PORT);
        return;
    }

    if (!m_db || !m_db->isOpen()) {
        ui->printerStatusLabel->setText(SettingsConstants::MSG_PRINTER_TEST_FAILED);
        return;
    }

    ThermalPrinter::ShopInfo shop;
    QSqlQuery query(m_db->database());
    query.prepare(Queries::PROFILE_SELECT);
    if (query.exec() && query.next()) {
        shop.name     = query.value("shop_name").toString();
        shop.phone    = query.value("phone").toString();
        shop.email    = query.value("email").toString();
        shop.logoPath = query.value("logo_path").toString();
    }

    QList<ThermalPrinter::TicketItem> items;
    items << ThermalPrinter::TicketItem{"Test Item A", 2, 5.00};
    items << ThermalPrinter::TicketItem{"Test Item B", 1, 12.50};
    items << ThermalPrinter::TicketItem{"Test Item C", 3, 3.75};

    const double subtotal       = 2*5.00 + 1*12.50 + 3*3.75;
    const double discountAmount = 3.00;
    const double totalAfterDisc = subtotal - discountAmount;

    ThermalPrinter::TicketData data;
    data.shop           = shop;
    data.items          = items;
    data.subtotal       = subtotal;
    data.discountAmount = discountAmount;
    data.total          = totalAfterDisc;
    data.isUnpaid       = false;

    ThermalPrinter printer(m_db->database());
    const bool ok = printer.print(data);

    ui->printerStatusLabel->setText(
        ok ? SettingsConstants::MSG_PRINTER_TEST_SENT
           : SettingsConstants::MSG_PRINTER_TEST_FAILED);
}
// ============================================================================
// APP SETTINGS — THEME
// ============================================================================

void SettingsWindow::loadAppSettings()
{
    // Block signals so setting combos don't trigger saveAppSettings
    QSignalBlocker blockColor(ui->colorSelect);
    QSignalBlocker blockFont(ui->fontSelect);
    QSignalBlocker blockBg(ui->bgSelect);
    QSignalBlocker blockOOS(ui->OOSCheckBox);

    QSqlDatabase db = m_db->database();
    const StyleManager::ColorScheme color = StyleManager::loadColorScheme(db);
    ui->colorSelect->setCurrentText(
        color == StyleManager::ColorScheme::Light ? "Light" : "Dark");

    const StyleManager::FontSize font = StyleManager::loadFontSize(db);
    switch (font) {
    case StyleManager::FontSize::Medium: ui->fontSelect->setCurrentText("Medium"); break;
    case StyleManager::FontSize::Large:  ui->fontSelect->setCurrentText("Large");  break;
    default:                             ui->fontSelect->setCurrentText("Small");  break;
    }

    const StyleManager::Background bg = StyleManager::loadBackground(db);
    switch (bg) {
    case StyleManager::Background::Caramel: ui->bgSelect->setCurrentText("Caramel"); break;
    case StyleManager::Background::Black:   ui->bgSelect->setCurrentText("Black");   break;
    case StyleManager::Background::Blue:    ui->bgSelect->setCurrentText("Blue");    break;
    default:                                ui->bgSelect->setCurrentText("Default"); break;
    }

    // OOS setting — default true (show out-of-stock items) if key not present
    bool showOOS = true;
    QSqlQuery query(db);
    query.exec("SELECT show_oos_products FROM app_settings WHERE id = 1");
    if (query.next()) {
        showOOS = query.value(0).toInt() != 0;
    }
    ui->OOSCheckBox->setChecked(showOOS);
}

void SettingsWindow::saveAppSettings()
{
    QSqlDatabase db = m_db->database();

    // Color
    const StyleManager::ColorScheme color =
        (ui->colorSelect->currentText() == "Light")
            ? StyleManager::ColorScheme::Light
            : StyleManager::ColorScheme::Dark;

    // Font
    StyleManager::FontSize font = StyleManager::FontSize::Small;
    const QString fontText = ui->fontSelect->currentText();
    if      (fontText == "Medium") font = StyleManager::FontSize::Medium;
    else if (fontText == "Large")  font = StyleManager::FontSize::Large;

    // Background
    StyleManager::Background bg = StyleManager::Background::Default;
    const QString bgText = ui->bgSelect->currentText();
    if      (bgText == "Caramel") bg = StyleManager::Background::Caramel;
    else if (bgText == "Black")   bg = StyleManager::Background::Black;
    else if (bgText == "Blue")    bg = StyleManager::Background::Blue;

    StyleManager::saveColorScheme(color, db);
    StyleManager::saveFontSize(font, db);
    StyleManager::saveBackground(bg, db);
    StyleManager::applyToAllWindows(db);

    // Save OOS setting to database
    QSqlQuery query(db);
    query.prepare("UPDATE app_settings SET show_oos_products = :oos WHERE id = 1");
    query.bindValue(":oos", ui->OOSCheckBox->isChecked() ? 1 : 0);
    query.exec();
}

// ============================================================================
// DATABASE — SYNC PATH
// ============================================================================

void SettingsWindow::loadSyncPath()
{
    QSqlQuery query(m_db->database());
    query.exec("SELECT sync_path, last_backup FROM app_settings WHERE id = 1");
    if (query.next()) {
        QString syncPath = query.value(0).toString();
        if (!syncPath.isEmpty())
            ui->syncPathPath->setText(syncPath);

        QString lastSync = query.value(1).toString();
        if (!lastSync.isEmpty())
            ui->lastSyncLabel->setText("Last backup: " + lastSync);
        else
            ui->lastSyncLabel->setText("Last backup: Never");
    }
}

void SettingsWindow::saveSyncPath(const QString& path)
{
    QSqlQuery query(m_db->database());
    query.prepare("UPDATE app_settings SET sync_path = :path WHERE id = 1");
    query.bindValue(":path", path);
    query.exec();
}

void SettingsWindow::onSyncPathBrowseClicked()
{
    const QString dir = QFileDialog::getExistingDirectory(
        this, "Select Backup Folder",
        ui->syncPathPath->text().isEmpty()
            ? QDir::homePath()
            : ui->syncPathPath->text());

    if (dir.isEmpty()) return;

    ui->syncPathPath->setText(dir);
    saveSyncPath(dir);
}

// ============================================================================
// DATABASE — HELPERS
// ============================================================================

QString SettingsWindow::sevenZipPath() const
{
#ifdef Q_OS_WIN
    const QStringList paths = {
        "C:/Program Files/7-Zip/7z.exe",
        "C:/Program Files (x86)/7-Zip/7z.exe",
        "C:/Program Files/7-Zip/7z.exe",
    };
    for (const QString& p : paths) {
        if (QFile::exists(p)) return p;
    }
    return "7z";
#else
    return "zip";
#endif
}

bool SettingsWindow::runZipProcess(const QStringList& args, const QString& toolName, QString* errorOut)
{
    QProcess process;
    process.start(toolName, args);

    if (!process.waitForStarted(5000)) {
        const QString err = "Failed to start " + toolName + ". Is it installed?";
        qWarning() << "runZipProcess:" << err;
        if (errorOut) *errorOut = err;
        return false;
    }

    if (!process.waitForFinished(60000)) {
        const QString err = "Process timed out";
        qWarning() << "runZipProcess:" << toolName << err;
        if (errorOut) *errorOut = err;
        process.kill();
        return false;
    }

    const int exitCode = process.exitCode();
    if (exitCode != 0) {
        const QString err = QString::fromLocal8Bit(process.readAllStandardError());
        qWarning() << "runZipProcess:" << toolName << "exited with code" << exitCode << err;
        if (errorOut) *errorOut = err.isEmpty() ? QString("Exited with code %1").arg(exitCode) : err;
        return false;
    }

    return true;
}

bool SettingsWindow::validateBackupZip(const QString& zipPath)
{
    // List zip contents and check for the two required marker files
    QProcess process;
    QStringList args;

#ifdef Q_OS_WIN
    args << "l" << zipPath;
    process.start(sevenZipPath(), args);
#else
    args << "-l" << zipPath;
    process.start("unzip", args);
#endif

    if (!process.waitForStarted(5000) || !process.waitForFinished(15000))
        return false;

    const QString output = QString::fromLocal8Bit(process.readAllStandardOutput());
    const bool hasDb       = output.contains(SettingsConstants::BACKUP_MARKER_DB);
    const bool hasSettings = output.contains(SettingsConstants::BACKUP_MARKER_SETTINGS);

    return hasDb && hasSettings;
}

// ============================================================================
// DATABASE — BACKUP
// ============================================================================

void SettingsWindow::onBackupClicked()
{
    const QString syncPath = ui->syncPathPath->text().trimmed();
    if (syncPath.isEmpty()) {
        QMessageBox::warning(this, "No Backup Folder",
                             SettingsConstants::MSG_BACKUP_PATH_MISSING);
        return;
    }

    if (!QDir(syncPath).exists()) {
        QMessageBox::warning(this, "Folder Not Found",
                             "The backup folder does not exist. Please select a valid folder.");
        return;
    }

    const QString dataPath  = POSConfig::getAppDataPath();
    const QString outputZip = syncPath + "/" + SettingsConstants::BACKUP_FILENAME;

    // Delete existing backup so we always have exactly one
    if (QFile::exists(outputZip))
        QFile::remove(outputZip);

    bool ok = false;

#ifdef Q_OS_WIN
    // 7z a -tzip "output.zip" "C:\path\to\POSBox\*"
    const QStringList args = {
        "a", "-tzip",
        QDir::toNativeSeparators(outputZip),
        QDir::toNativeSeparators(dataPath + "/*")
    };
    const QString tool = sevenZipPath();
    QString errMsg;
    ok = runZipProcess(args, tool, &errMsg);
    if (!ok) {
        QMessageBox::warning(this, "Backup Failed",
                             QString("Failed to create backup archive (7-Zip).\n\n%1\n\nMake sure 7-Zip is installed at:\n- C:/Program Files/7-Zip/7z.exe\n- C:/Program Files (x86)/7-Zip/7z.exe")
                             .arg(errMsg));
        return;
    }
#else
    // zip -r "output.zip" "." from inside the data directory
    QProcess process;
    process.setWorkingDirectory(dataPath);
    const QStringList args = { "-r", outputZip, "." };
    process.start("zip", args);
    if (!process.waitForStarted(5000) || !process.waitForFinished(60000)
        || process.exitCode() != 0) {
        QMessageBox::warning(this, "Backup Failed",
                             QString(SettingsConstants::MSG_BACKUP_ZIP_FAILED).arg("zip"));
        return;
    }
    ok = true;
#endif

    if (ok) {
        // Save timestamp to database
        const QString timestamp = QDateTime::currentDateTime()
                                      .toString("yyyy-MM-dd HH:mm");

        QSqlQuery query(m_db->database());
        query.prepare("UPDATE app_settings SET last_backup = :backup WHERE id = 1");
        query.bindValue(":backup", timestamp);
        query.exec();

        ui->lastSyncLabel->setText("Last backup: " + timestamp);
        QMessageBox::information(this, "Backup Complete",
                                 SettingsConstants::MSG_BACKUP_SUCCESS);
    }
}

// ============================================================================
// DATABASE — IMPORT
// ============================================================================

void SettingsWindow::onImportClicked()
{
    // 1 — Pick zip file
    const QString zipPath = QFileDialog::getOpenFileName(
        this, "Select POSBox Backup",
        ui->syncPathPath->text().isEmpty()
            ? QDir::homePath()
            : ui->syncPathPath->text(),
        "POSBox Backup (*.zip)");

    if (zipPath.isEmpty()) return;

    // 2 — Validate it's a real POSBox backup
    if (!validateBackupZip(zipPath)) {
        QMessageBox::warning(this, "Invalid Backup",
                             SettingsConstants::MSG_IMPORT_INVALID_ZIP);
        return;
    }

    // 3 — Confirm
    const int confirm = QMessageBox::warning(
        this, "Confirm Import",
        SettingsConstants::MSG_IMPORT_CONFIRM,
        QMessageBox::Yes | QMessageBox::No);

    if (confirm != QMessageBox::Yes) return;

    // 4 — Extract into app data folder
    const QString dataPath = POSConfig::getAppDataPath();
    bool ok = false;

#ifdef Q_OS_WIN
    // 7z x "file.zip" -o"dest" -y  (overwrite without prompting)
    const QStringList args = {
        "x",
        QDir::toNativeSeparators(zipPath),
        "-o" + QDir::toNativeSeparators(dataPath),
        "-y"
    };
    ok = runZipProcess(args, sevenZipPath(), nullptr);
#else
    // unzip -o "file.zip" -d "dest"
    const QStringList args = { "-o", zipPath, "-d", dataPath };
    ok = runZipProcess(args, "unzip", nullptr);
#endif

    if (!ok) {
        QMessageBox::warning(this, "Import Failed",
                             SettingsConstants::MSG_IMPORT_FAILED);
        return;
    }

    // 5 — Success: notify and quit
    QMessageBox::information(this, "Import Successful",
                             SettingsConstants::MSG_IMPORT_SUCCESS);
    QApplication::quit();
}

// ============================================================================
// NOTIFICATIONS SETTINGS
// ============================================================================

void SettingsWindow::loadNotificationSettings()
{
    QSignalBlocker blocker1(ui->openTicketCheck);
    QSignalBlocker blocker2(ui->openTicketDays);
    QSignalBlocker blocker3(ui->openTicketFrequancy);
    QSignalBlocker blocker4(ui->outStockCheck);
    QSignalBlocker blocker5(ui->nearOutStockCheck);
    Q_UNUSED(blocker1)
    Q_UNUSED(blocker2)
    Q_UNUSED(blocker3)
    Q_UNUSED(blocker4)
    Q_UNUSED(blocker5)

    QSqlQuery query(m_db->database());
    query.exec("SELECT open_ticket_enabled, open_ticket_days, open_ticket_frequency, expense_due_enabled, expense_due_days, expense_overdue_enabled, expense_overdue_days, out_of_stock_enabled, nearly_out_of_stock_enabled FROM notification_settings WHERE id = 1");
    if (!query.next())
        return;

    ui->openTicketCheck->setChecked(query.value(0).toInt() != 0);
    ui->openTicketDays->setCurrentIndex(ui->openTicketDays->findData(query.value(1).toInt()));
    ui->openTicketFrequancy->setCurrentIndex(ui->openTicketFrequancy->findData(query.value(2).toInt()));

    ui->outStockCheck->setChecked(query.value(7).toInt() != 0);
    ui->nearOutStockCheck->setChecked(query.value(8).toInt() != 0);
}

void SettingsWindow::saveNotificationSettings()
{
    QSqlQuery query(m_db->database());
    query.prepare(R"(
        UPDATE notification_settings SET
            open_ticket_enabled = :ot_enabled,
            open_ticket_days = :ot_days,
            open_ticket_frequency = :ot_freq,
            expense_due_enabled = :ed_enabled,
            expense_due_days = :ed_days,
            expense_overdue_enabled = :eo_enabled,
            expense_overdue_days = :eo_days,
            out_of_stock_enabled = :oos_enabled,
            nearly_out_of_stock_enabled = :nos_enabled
        WHERE id = 1
    )");
    query.bindValue(":ot_enabled", ui->openTicketCheck->isChecked() ? 1 : 0);
    query.bindValue(":ot_days", ui->openTicketDays->currentData().toInt());
    query.bindValue(":ot_freq", ui->openTicketFrequancy->currentData().toInt());
    query.bindValue(":oos_enabled", ui->outStockCheck->isChecked() ? 1 : 0);
    query.bindValue(":nos_enabled", ui->nearOutStockCheck->isChecked() ? 1 : 0);
    query.exec();
}
