#ifndef USERDIALOG_H
#define USERDIALOG_H

#include <QDialog>

class DatabaseManager;
class QLineEdit;
class QComboBox;
class QCheckBox;
class QLabel;

class UserDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Mode { Add, Edit };

    explicit UserDialog(DatabaseManager* db, Mode mode,
                        const QString& username = QString(),
                        QWidget* parent = nullptr);
    ~UserDialog() = default;

private slots:
    void onShowPasswordToggled(bool checked);
    void onSaveClicked();
    void onCancelClicked();

private:
    void setupUI();
    void loadUser();
    bool validate();
    bool saveUser();

    DatabaseManager* m_db;
    Mode             m_mode;
    QString          m_username; // pre-set in edit mode

    // Fields
    QLineEdit* m_usernameField;
    QLineEdit* m_displayNameField;
    QComboBox* m_roleCombo;
    QLineEdit* m_passwordField;
    QLineEdit* m_confirmPasswordField;
    QCheckBox* m_showPasswordCheck;
};

#endif // USERDIALOG_H
