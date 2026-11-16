#ifndef SETTINGSWINDOW_H
#define SETTINGSWINDOW_H

#include <QMainWindow>
#include <QEvent>
#include <QModelIndex>
#include "CountryData.h"

class UserSession;
class LocalApi;
class LocalImages;
class QLineEdit;
class QLabel;
class QStandardItemModel;
class QColor;
class QComboBox;

QT_BEGIN_NAMESPACE
namespace Ui {
class SettingsWindow;
}
QT_END_NAMESPACE

class QPushButton;

class ReceiptDesignPage;

class SettingsWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit SettingsWindow(UserSession* session, LocalApi* api,
                            LocalImages* images, QWidget* parent = nullptr);
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

    // Users
    void onUserAddClicked();
    void onUserEditClicked();
    void onUserDeleteClicked();
    void onUserSelectionChanged(const QModelIndex& current, const QModelIndex& previous);
    void onPermissionsSaveClicked();

    // App Settings — background color picker (fallback when no image is set)
    void onBgColorClicked();

    // App Settings — background image picker
    void onBgImageChooseClicked();
    void onBgImageRemoveClicked();

    // Printer
    void onSavePrinterBtn();
    void onPrintTestBtn();
    void onPortRefreshBtn();
    void onPrinterEnableToggled(bool enabled);

    // Region & language
    void onCountryChanged(int index);
    void onCurrencyChanged(int index);
    void onLanguageChanged(int index);

    // Backup & restore (local, manual)
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

    // Sidebar layout (SettingsConsoleLayout.cpp)
    void buildConsoleLayout();
    void updateIcons();

    // Shop Profile
    void loadProfile();
    bool saveProfile();
    bool validateFields();
    void setFieldState(QLineEdit* field, bool valid);
    void clearFieldState(QLineEdit* field);
    bool isPhoneValid() const;
    bool isPostalValid() const;
    bool isEmailValid() const;
    void displayPixmap(const QPixmap& pixmap);
    void showLogoPlaceholder();

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
    void applyBackgroundColor(const QColor& color);
    void updateBgSwatch(const QString& hex);
    void applyBackgroundImageOptions();
    void reapplyThemeToSelf();
    void updateBgImagePreview();

    // Region & language
    void createRegionalControls();
    void populateCountryCurrencyCombos();
    void loadCountryCurrency();
    void saveCountryCurrency();
    void updateRegionalFields();
    void populateLanguageCombo();
    void loadLanguage();

    // Backup & restore
    void createBackupControls();
    void loadSyncPath();
    void saveSyncPath(const QString& path);
    QString zipToolPath() const;
    bool runZipProcess(const QStringList& args, const QString& tool, QString* errorOut,
                       const QString& workingDir = QString());
    bool validateBackupZip(const QString& zipPath);

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
    ReceiptDesignPage* m_receiptPage = nullptr;
    UserSession*        m_session;
    LocalApi*          m_api;
    LocalImages*       m_images;

    QString m_newLogoSourcePath;
    QString m_existingLogoFilename;

    QStandardItemModel* m_usersModel;

    // Region & language (created in code)
    QComboBox* m_countrySelect = nullptr;
    QComboBox* m_currencySelect = nullptr;
    QComboBox* m_languageSelect = nullptr;
    QLabel* m_phonePrefix = nullptr;
    CountryInfo m_currentCountry;

    // Backup & restore (created in code)
    QLineEdit* m_syncPathEdit = nullptr;
    QPushButton* m_syncPathBtn = nullptr;
    QPushButton* m_backupBtn = nullptr;
    QPushButton* m_restoreBtn = nullptr;
    QLabel* m_lastBackupLabel = nullptr;

    struct NavIcon { QPushButton* button; QString name; };
    QList<NavIcon> m_navIcons;     // follow the accent color when active
    QList<NavIcon> m_textIcons;    // plain buttons, text color
    QList<NavIcon> m_whiteIcons;   // colored buttons
};

#endif // SETTINGSWINDOW_H
