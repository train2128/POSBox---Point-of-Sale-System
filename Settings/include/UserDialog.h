#ifndef USERDIALOG_H
#define USERDIALOG_H

#include "ModernDialog.h"

class LocalApi;
class QLineEdit;
class QComboBox;
class QCheckBox;
class QLabel;

class UserDialog : public ModernDialog
{
    Q_OBJECT

public:
    enum class Mode { Add, Edit };

    explicit UserDialog(LocalApi* api, Mode mode,
                        const QString& username = QString(),
                        QWidget* parent = nullptr);
    ~UserDialog() = default;

private slots:
    void onShowPasswordToggled(bool checked);
    void onSaveClicked();
    void updatePasswordMatch();

private:
    void setupUI();
    void loadUser();
    bool validate();
    bool saveUser();

    LocalApi* m_api;
    Mode             m_mode;
    QString          m_username; // pre-set in edit mode

    // Fields
    QLineEdit* m_usernameField;
    QLineEdit* m_displayNameField;
    QComboBox* m_roleCombo;
    QLineEdit* m_passwordField;
    QLineEdit* m_confirmPasswordField;
    QCheckBox* m_showPasswordCheck;
    QLabel*    m_matchLabel;
};

#endif // USERDIALOG_H
