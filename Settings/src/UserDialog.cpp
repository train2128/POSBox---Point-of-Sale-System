#include "UserDialog.h"
#include "Prefixes.h"
#include "ToggleSwitch.h"
#include "SettingsConstants.h"
#include "LocalApi.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QStyle>
#include <QDialogButtonBox>
#include <QDebug>

UserDialog::UserDialog(LocalApi* api, Mode mode,
                       const QString& username, QWidget* parent)
    : ModernDialog(Look::Card, "user", Tone::Accent, mode == Mode::Add ? tr("Add user") : tr("Edit user"),
                   mode == Mode::Add ? tr("They sign in with this username and password")
                                     : tr("Leave the password empty to keep the current one"),
                   420, parent)
    , m_api(api)
    , m_mode(mode)
    , m_username(username)
    , m_usernameField(nullptr)
    , m_displayNameField(nullptr)
    , m_roleCombo(nullptr)
    , m_passwordField(nullptr)
    , m_confirmPasswordField(nullptr)
    , m_showPasswordCheck(nullptr)
    , m_matchLabel(nullptr)
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
    m_usernameField = new QLineEdit(this);
    m_usernameField->setPlaceholderText(tr("Username"));
    if (m_mode == Mode::Edit) {
        m_usernameField->setText(m_username);
        m_usernameField->setReadOnly(true);
        m_usernameField->setProperty("locked", true);
    }

    m_displayNameField = new QLineEdit(this);
    m_displayNameField->setPlaceholderText(tr("Display name"));
    addFieldPair(tr("Username"), m_usernameField, tr("Display name"), m_displayNameField);

    m_roleCombo = new QComboBox(this);
    // The stored role travels as item data; the shown name is translated.
    for (const char* role : {"Admin", "Manager", "Cashier"})
        m_roleCombo->addItem(AppHelpers::roleName(role), QString::fromLatin1(role));
    addField(tr("Role"), m_roleCombo);

    m_passwordField = new QLineEdit(this);
    m_passwordField->setEchoMode(QLineEdit::Password);
    m_passwordField->setPlaceholderText(m_mode == Mode::Edit ? tr("Keep current") : tr("Password"));

    m_confirmPasswordField = new QLineEdit(this);
    m_confirmPasswordField->setEchoMode(QLineEdit::Password);
    m_confirmPasswordField->setPlaceholderText(tr("Confirm password"));
    addFieldPair(tr("Password"), m_passwordField, tr("Confirm password"), m_confirmPasswordField);

    auto* showRow = new QHBoxLayout;
    auto* showLabel = new QLabel(tr("Show password"), this);
    showLabel->setObjectName("modalSwitchLabel");
    m_showPasswordCheck = new ToggleSwitch(this);
    showRow->addWidget(showLabel, 1);
    showRow->addWidget(m_showPasswordCheck);
    body()->addLayout(showRow);

    m_matchLabel = new QLabel(this);
    m_matchLabel->setObjectName("modalMatch");
    body()->addWidget(m_matchLabel);
    updatePasswordMatch();

    QPushButton* saveBtn = addFooter(m_mode == Mode::Add ? tr("Add User") : tr("Save"),
                                     m_mode == Mode::Add ? "plus" : "check", "add");

    connect(m_showPasswordCheck, &QCheckBox::toggled,
            this, &UserDialog::onShowPasswordToggled);
    connect(m_passwordField, &QLineEdit::textChanged, this, &UserDialog::updatePasswordMatch);
    connect(m_confirmPasswordField, &QLineEdit::textChanged, this, &UserDialog::updatePasswordMatch);
    connect(saveBtn, &QPushButton::clicked, this, &UserDialog::onSaveClicked);
}

void UserDialog::updatePasswordMatch()
{
    const QString password = m_passwordField->text();
    const QString confirm = m_confirmPasswordField->text();
    const bool answered = !password.isEmpty() && !confirm.isEmpty();
    const bool match = password == confirm;
    m_matchLabel->setText(answered ? (match ? tr("✓  Passwords match") : tr("Passwords don't match")) : QString());
    m_matchLabel->setProperty("bad", answered && !match);
    m_matchLabel->style()->unpolish(m_matchLabel);
    m_matchLabel->style()->polish(m_matchLabel);
}

// ============================================================================
// LOAD (edit mode)
// ============================================================================

void UserDialog::loadUser()
{
    QList<DataRow> rows = m_api->select("accounts", {{"username", m_username}});
    if (!m_api->ok() || rows.isEmpty()) {
        qWarning() << "UserDialog: failed to load user:" << m_username
                   << m_api->lastError();
        return;
    }
    DataRow row = rows.first();

    m_displayNameField->setText(row.value("display_name").toString());

    QString role = row.value("role").toString();
    int roleIndex = m_roleCombo->findData(role);
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
        AppHelpers::showWarning(this, tr("Username cannot be empty."));
        m_usernameField->setFocus();
        return false;
    }

    if (m_displayNameField->text().trimmed().isEmpty()) {
        AppHelpers::showWarning(this, tr("Display name cannot be empty."));
        m_displayNameField->setFocus();
        return false;
    }

    QString password = m_passwordField->text();
    QString confirm  = m_confirmPasswordField->text();

    // In add mode password is required; in edit mode it's optional
    if (m_mode == Mode::Add && password.isEmpty()) {
        AppHelpers::showWarning(this, tr("Password cannot be empty."));
        m_passwordField->setFocus();
        return false;
    }

    if (!password.isEmpty() && password != confirm) {
        AppHelpers::showWarning(this, tr("Passwords do not match."));
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
    QString role        = m_roleCombo->currentData().toString();
    QString password    = m_passwordField->text();

    if (m_mode == Mode::Add) {
        int id = m_api->insert("accounts", {
            {"username", username}, {"password_hash", password},
            {"role", role}, {"display_name", displayName}
        });
        if (id < 0 && !m_api->ok()) {
            qWarning() << "UserDialog: insert failed:" << m_api->lastError();
            AppHelpers::showWarning(this, tr("Could not create the user.\n\n") + m_api->lastError());
            return false;
        }

        qDebug() << "User added:" << username;

    } else {
        // Password only updated when a new one was entered.
        QVariantMap values{{"role", role}, {"display_name", displayName}};
        if (!password.isEmpty())
            values["password_hash"] = password;
        if (!m_api->update("accounts", values, {{"username", m_username}})) {
            qWarning() << "UserDialog: update failed:" << m_api->lastError();
            AppHelpers::showWarning(this, tr("Could not update the user.\n\n") + m_api->lastError());
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
