#ifndef SETTINGSWINDOW_H
#define SETTINGSWINDOW_H

#include <QMainWindow>
#include <QEvent>
#include <QModelIndex>

class UserSession;
class DatabaseManager;
class PermissionManager;
class QLineEdit;
class QLabel;
class QStandardItemModel;

QT_BEGIN_NAMESPACE
namespace Ui {
class SettingsWindow;
}
QT_END_NAMESPACE

class SettingsWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit SettingsWindow(UserSession* session, DatabaseManager* db, QWidget* parent = nullptr);
    ~SettingsWindow();

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void onLogoutClicked();

    // Shop Profile
    void onLogoClicked();
    void onSaveClicked();
    void onCancelClicked();
    void onPhoneChanged(const QString& text);
    void onPostalChanged(const QString& text);
    void onEmailChanged(const QString& text);
    void onTaxIdChanged(const QString& text);

    // Users
    void onUserAddClicked();
    void onUserEditClicked();
    void onUserDeleteClicked();
    void onUserSelectionChanged(const QModelIndex& current, const QModelIndex& previous);
    void onPermissionsSaveClicked();

    // Printer
    void onSavePrinterBtn();
    void onPrintTestBtn();
    void onPortRefreshBtn();
    void onPrinterEnableToggled(bool enabled);

    // Database
    void onSyncPathBrowseClicked();
    void onBackupClicked();
    void onImportClicked();

signals:
    void logoutRequested();

private:
    // Setup
    void setupUI();
    void setupValidators();
    void setupConnections();
    void updateUserDisplay();

    // Shop Profile
    void loadProfile();
    bool saveProfile();
    bool validateFields();
    void setFieldState(QLineEdit* field, bool valid);
    void clearFieldState(QLineEdit* field);
    bool isPhoneValid() const;
    bool isPostalValid() const;
    bool isEmailValid() const;
    void displayLogo(const QString& fullPath);

    // Users
    void loadUsers();
    QString selectedUsername() const;
    void updatePermissionsPanel(const QString& username);
    void loadUserPermissions(const QString& username);
    void saveUserPermissions(const QString& username);
    void clearPermissionsPanel();

    // App Settings
    void loadAppSettings();
    void saveAppSettings();

    // Database
    void loadSyncPath();
    void saveSyncPath(const QString& path);
    bool runZipProcess(const QStringList& args, const QString& toolName, QString* errorOut = nullptr);
    bool validateBackupZip(const QString& zipPath);
    QString sevenZipPath() const;

    // Printer
    void populatePrinterCombos();
    void scanComPorts();
    void loadPrinterSettings();
    void savePrinterSettings();

    // Notifications
    void loadNotificationSettings();
    void saveNotificationSettings();

    void setPrinterControlsEnabled(bool enabled);

    Ui::SettingsWindow* ui;
    UserSession*        m_session;
    DatabaseManager*    m_db;
    PermissionManager* m_permissionManager;

    QString m_newLogoSourcePath;
    QString m_existingLogoFilename;

    QStandardItemModel* m_usersModel;
};

#endif // SETTINGSWINDOW_H
