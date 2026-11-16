#ifndef LAUNCHERWINDOW_H
#define LAUNCHERWINDOW_H

#include <QDialog>
#include <QList>
#include <QString>

class ChangeNotifier;
class QAction;
class QCloseEvent;
class QLabel;
class QLineEdit;
class QMenu;
class QPushButton;
class QSystemTrayIcon;
class QThread;
class QTimer;
class LocalApi;

/**
 * The Launcher is the single entry point into POSBox. It:
 *   - acts as the sole writer of the shared active-session state,
 *   - authenticates the user and lets them pick which app to open,
 *   - stays in the system tray while apps run,
 *   - routes cross-app navigation (opening an app, open-ticket, expense,
 *     product) by launching the target app with the current session.
 */
class LauncherWindow : public QDialog
{
    Q_OBJECT

public:
    LauncherWindow(LocalApi* api, QSystemTrayIcon* tray);
    ~LauncherWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onLoginClicked();
    void onAppClicked(int index);
    void onPollApps();
    void onLogoutTray();
    void onExitTray();
    void quitEntirely();

private:
    void setupUI();
    void setupTray();
    void setupNotifier();
    void setupPolling();
    QWidget* buildLoginPage();
    QWidget* buildWelcomePage();
    QWidget* buildFooter();
    void setBusy(bool busy);
    void showLoginError(const QString& message);
    void clearLoginError();
    void refreshLoginButton();
    void finishLogin(const QString& username, const QString& role,
                     const QString& displayName, const QString& error);
    void retranslateUi();
    void doLogoutAndShowLogin();
    void handleAppOpenRequest(const QString& request);
    void showLoginState();
    void showWelcomeState();
    void hideToTray();
    bool isLoggedIn() const { return m_loggedIn; }
    void forceKillAllApps();
    void clearAppLocks();

    LocalApi* m_api;
    QSystemTrayIcon* m_tray;
    ChangeNotifier* m_notifier = nullptr;
    QTimer* m_pollTimer = nullptr;
    QMenu* m_trayMenu = nullptr;

    QLineEdit* m_userEdit = nullptr;
    QLineEdit* m_passEdit = nullptr;
    QPushButton* m_loginBtn = nullptr;
    QPushButton* m_logoutBtn = nullptr;
    QLabel* m_welcomeLabel = nullptr;
    QLabel* m_avatarLabel = nullptr;
    QLabel* m_roleLabel = nullptr;
    QLabel* m_loginMessage = nullptr;
    QWidget* m_card = nullptr;
    QWidget* m_loginPage = nullptr;
    QWidget* m_welcomePage = nullptr;
    QLabel* m_subtitle = nullptr;
    QLabel* m_userCaption = nullptr;
    QLabel* m_passCaption = nullptr;
    QLabel* m_appsCaption = nullptr;
    QList<QLabel*> m_appNames;
    QPushButton* m_coffeeBtn = nullptr;
    QPushButton* m_closeBtn = nullptr;
    QAction* m_showPassAction = nullptr;
    QTimer* m_spinTimer = nullptr;
    QList<QPushButton*> m_appButtons;

    bool m_loggedIn = false;
    bool m_busy = false;
    int m_spinAngle = 0;
    int m_prevRunning = 0;
    QString m_username;
    QString m_role;
    QString m_displayName;
};

#endif // LAUNCHERWINDOW_H
