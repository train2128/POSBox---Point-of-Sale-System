#include "UserDialog.h"
#include "SettingsConstants.h"
#include "DatabaseManager.h"
#include "QueryRepository.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QDialogButtonBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QMessageBox>
#include <QDebug>

UserDialog::UserDialog(DatabaseManager* db, Mode mode,
                       const QString& username, QWidget* parent)
    : QDialog(parent)
    , m_db(db)
    , m_mode(mode)
    , m_username(username)
    , m_usernameField(nullptr)
    , m_displayNameField(nullptr)
    , m_roleCombo(nullptr)
    , m_passwordField(nullptr)
    , m_confirmPasswordField(nullptr)
    , m_showPasswordCheck(nullptr)
{
    setupUI();
    if (m_mode == Mode::Edit) {
        loadUser();
    }
}

// ============================================================================
// SETUP
// ============================================================================

void UserDialog::setupUI()
{
    setWindowTitle(m_mode == Mode::Add ? tr("Add User") : tr("Edit User"));
    setMinimumWidth(380);
    setModal(true);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(16);
    mainLayout->setContentsMargins(24, 24, 24, 24);

    // Title label
    QLabel* titleLabel = new QLabel(
        m_mode == Mode::Add ? tr("Add New User") : tr("Edit User"), this);
    titleLabel->setStyleSheet(
        "font-size: 15px; font-weight: bold; color: #ffffff; margin-bottom: 4px;");
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);

    // Form layout
    QFormLayout* form = new QFormLayout();
    form->setSpacing(10);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    auto makeLabel = [&](const QString& text) -> QLabel* {
        QLabel* lbl = new QLabel(text, this);
        lbl->setStyleSheet("font-size: 13px; font-weight: bold; color: #cccccc;");
        return lbl;
    };

    // Username
    m_usernameField = new QLineEdit(this);
    m_usernameField->setPlaceholderText(tr("Username"));
    m_usernameField->setAlignment(Qt::AlignCenter);
    if (m_mode == Mode::Edit) {
        m_usernameField->setText(m_username);
        m_usernameField->setReadOnly(true);
        m_usernameField->setStyleSheet(
            "QLineEdit {"
            "  background-color: #2d2d2d;"
            "  color: #888888;"
            "  border: 2px solid #404040;"
            "  border-radius: 8px;"
            "  padding: 8px 12px;"
            "  font-size: 13px;"
            "}");
    }
    form->addRow(makeLabel(tr("Username")), m_usernameField);

    // Display name
    m_displayNameField = new QLineEdit(this);
    m_displayNameField->setPlaceholderText(tr("Display name"));
    m_displayNameField->setAlignment(Qt::AlignCenter);
    form->addRow(makeLabel(tr("Display Name")), m_displayNameField);

    // Role
    m_roleCombo = new QComboBox(this);
    m_roleCombo->addItems({tr("Admin"), tr("Manager"), tr("Cashier")});
    form->addRow(makeLabel(tr("Role")), m_roleCombo);

    // Password
    m_passwordField = new QLineEdit(this);
    m_passwordField->setEchoMode(QLineEdit::Password);
    m_passwordField->setAlignment(Qt::AlignCenter);
    if (m_mode == Mode::Edit) {
        m_passwordField->setPlaceholderText(tr("Leave empty to keep current password"));
    } else {
        m_passwordField->setPlaceholderText(tr("Password"));
    }
    form->addRow(makeLabel(tr("Password")), m_passwordField);

    // Confirm password
    m_confirmPasswordField = new QLineEdit(this);
    m_confirmPasswordField->setEchoMode(QLineEdit::Password);
    m_confirmPasswordField->setAlignment(Qt::AlignCenter);
    m_confirmPasswordField->setPlaceholderText(tr("Confirm password"));
    form->addRow(makeLabel(tr("Confirm")), m_confirmPasswordField);

    mainLayout->addLayout(form);

    // Show password checkbox
    m_showPasswordCheck = new QCheckBox(tr("Show password"), this);
    m_showPasswordCheck->setStyleSheet(
        "QCheckBox { color: #aaaaaa; font-size: 12px; }");
    mainLayout->addWidget(m_showPasswordCheck);

    // Buttons
    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(10);

    QPushButton* cancelBtn = new QPushButton(tr("Cancel"), this);
    QPushButton* saveBtn   = new QPushButton(
        m_mode == Mode::Add ? tr("Add") : tr("Save"), this);

    saveBtn->setProperty("btnStyle", "add");
    saveBtn->setMinimumHeight(36);
    cancelBtn->setMinimumHeight(36);

    btnLayout->addWidget(cancelBtn);
    btnLayout->addWidget(saveBtn);
    mainLayout->addLayout(btnLayout);

    // Connections
    connect(m_showPasswordCheck, &QCheckBox::toggled,
            this, &UserDialog::onShowPasswordToggled);
    connect(saveBtn,   &QPushButton::clicked, this, &UserDialog::onSaveClicked);
    connect(cancelBtn, &QPushButton::clicked, this, &UserDialog::onCancelClicked);
}

// ============================================================================
// LOAD (edit mode)
// ============================================================================

void UserDialog::loadUser()
{
    QSqlQuery query(m_db->database());
    query.prepare(Queries::ACCOUNT_SELECT_BY_USERNAME);
    query.addBindValue(m_username);

    if (!query.exec() || !query.next()) {
        qWarning() << "UserDialog: failed to load user:" << m_username
                   << query.lastError().text();
        return;
    }

    m_displayNameField->setText(query.value("display_name").toString());

    QString role = query.value("role").toString();
    int roleIndex = m_roleCombo->findText(role);
    if (roleIndex >= 0) {
        m_roleCombo->setCurrentIndex(roleIndex);
    }

    // Password fields are intentionally left empty in edit mode
}

// ============================================================================
// VALIDATE
// ============================================================================

bool UserDialog::validate()
{
    if (m_usernameField->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("Validation Error"), tr("Username cannot be empty."));
        m_usernameField->setFocus();
        return false;
    }

    if (m_displayNameField->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("Validation Error"), tr("Display name cannot be empty."));
        m_displayNameField->setFocus();
        return false;
    }

    QString password = m_passwordField->text();
    QString confirm  = m_confirmPasswordField->text();

    // In add mode password is required; in edit mode it's optional
    if (m_mode == Mode::Add && password.isEmpty()) {
        QMessageBox::warning(this, tr("Validation Error"), tr("Password cannot be empty."));
        m_passwordField->setFocus();
        return false;
    }

    if (!password.isEmpty() && password != confirm) {
        QMessageBox::warning(this, tr("Validation Error"), tr("Passwords do not match."));
        m_confirmPasswordField->clear();
        m_confirmPasswordField->setFocus();
        return false;
    }

    return true;
}

// ============================================================================
// SAVE
// ============================================================================

bool UserDialog::saveUser()
{
    QString username    = m_usernameField->text().trimmed();
    QString displayName = m_displayNameField->text().trimmed();
    QString role        = m_roleCombo->currentText();
    QString password    = m_passwordField->text();

    if (m_mode == Mode::Add) {
        QSqlQuery query(m_db->database());
        query.prepare(Queries::ACCOUNT_INSERT);
        query.addBindValue(username);
        query.addBindValue(password);
        query.addBindValue(role);
        query.addBindValue(displayName);
        query.addBindValue(QVariant()); // email
        query.addBindValue(QVariant()); // phone

        if (!query.exec()) {
            qWarning() << "UserDialog: insert failed:" << query.lastError().text();
            QMessageBox::warning(this, tr("Error"), tr(SettingsConstants::MSG_USER_ADD_SUCCESS));
            return false;
        }

        qDebug() << "User added:" << username;

    } else {
        // Build update query — password only updated if a new one was entered
        QSqlQuery query(m_db->database());

        if (password.isEmpty()) {
            // Keep existing password — update everything else
            query.prepare(
                "UPDATE accounts SET role = ?, display_name = ?, is_active = 1 "
                "WHERE username = ?");
            query.addBindValue(role);
            query.addBindValue(displayName);
            query.addBindValue(m_username);
        } else {
            query.prepare(
                "UPDATE accounts SET password_hash = ?, role = ?, display_name = ?, is_active = 1 "
                "WHERE username = ?");
            query.addBindValue(password);
            query.addBindValue(role);
            query.addBindValue(displayName);
            query.addBindValue(m_username);
        }

        if (!query.exec()) {
            qWarning() << "UserDialog: update failed:" << query.lastError().text();
            QMessageBox::warning(this, tr("Error"), tr(SettingsConstants::MSG_USER_EDIT_SUCCESS));
            return false;
        }

        qDebug() << "User updated:" << m_username;
    }

    return true;
}

// ============================================================================
// SLOTS
// ============================================================================

void UserDialog::onShowPasswordToggled(bool checked)
{
    QLineEdit::EchoMode mode = checked ? QLineEdit::Normal : QLineEdit::Password;
    m_passwordField->setEchoMode(mode);
    m_confirmPasswordField->setEchoMode(mode);
}

void UserDialog::onSaveClicked()
{
    if (!validate()) return;
    if (!saveUser()) return;
    accept();
}

void UserDialog::onCancelClicked()
{
    reject();
}
