#include "LauncherWindow.h"
#include "AccessControl.h"
#include "ActiveSessionManager.h"
#include "Animations.h"
#include "AppLock.h"
#include "ChangeNotifier.h"
#include "CommonConfig.h"
#include "LocalSettings.h"
#include "Prefixes.h"
#include "LocalApi.h"
#include "TranslationManager.h"
#include "StyleManager.h"
#include "LineIcons.h"
#include "ModernDialog.h"
#include "TableDelegates.h"
#include <QAction>
#include <QApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QCloseEvent>
#include <QDateTime>
#include <QDir>
#include <QHBoxLayout>
#include <QIcon>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QFrame>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QProcess>
#include <QPushButton>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QThread>
#include <QTimer>
#include <QWindow>
#include <QVBoxLayout>

#ifndef Q_OS_WIN
#include <signal.h>
#endif

namespace {

// Constants
const char* LOGO_ICON      = ":/Icons/images/icons/launcher_icon.png";
const char* COFFEE_URL     = "https://buymeacoffee.com/cheriff";

const char* APP_ICONS[] = {
    ":/Icons/images/icons/register_icon_colored.png",
    ":/Icons/images/icons/tickets_icon_colored.png",
    ":/Icons/images/icons/inventory_icon_colored.png",
    ":/Icons/images/icons/financial_icon_colored.png",
    ":/Icons/images/icons/settings_icon_colored.png"
};

// Executable / lock names (never translated) and the names shown to the user.
const char* APP_NAMES[] = { "Register", "Tickets", "Inventory", "Financial", "Settings" };
const char* APP_TITLES[] = {
    QT_TRANSLATE_NOOP("LauncherWindow", "Register"),
    QT_TRANSLATE_NOOP("LauncherWindow", "Tickets"),
    QT_TRANSLATE_NOOP("LauncherWindow", "Inventory"),
    QT_TRANSLATE_NOOP("LauncherWindow", "Financial"),
    QT_TRANSLATE_NOOP("LauncherWindow", "Settings")
};
const char* APP_LOCK_FILE[] = {
    "register.lock", "tickets.lock", "inventory.lock", "financial.lock", "settings.lock"
};
const AccessControl::Page APP_PAGES[] = {
    AccessControl::RegisterPage,
    AccessControl::TicketsPage,
    AccessControl::InventoryPage,
    AccessControl::FinancesPage,
    AccessControl::SettingsPage
};

const int APP_COUNT = 5;

constexpr int CardWidth = 420;
constexpr int CardRadius = 16;
constexpr int SpinnerStepMs = 40;

// A white icon that stays white while its button is disabled.
QIcon whiteIcon(const QPixmap& pixmap)
{
    QIcon icon;
    icon.addPixmap(pixmap, QIcon::Normal);
    icon.addPixmap(pixmap, QIcon::Disabled);
    return icon;
}

QPixmap spinnerPixmap(int angle)
{
    constexpr int Size = 18;
    constexpr qreal Scale = 3.0;
    QPixmap px(QSize(Size, Size) * Scale);
    px.setDevicePixelRatio(Scale);
    px.fill(Qt::transparent);
    QPainter painter(&px);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF ring(2, 2, Size - 4, Size - 4);
    painter.setPen(QPen(QColor(255, 255, 255, 90), 2));
    painter.drawEllipse(ring);
    painter.setPen(QPen(Qt::white, 2, Qt::SolidLine, Qt::RoundCap));
    painter.drawArc(ring, angle * 16, 110 * 16);
    return px;
}

void repolish(QWidget* widget)
{
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
}

} // namespace

LauncherWindow::LauncherWindow(LocalApi* api, QSystemTrayIcon* tray)
    : QDialog(nullptr, Qt::Dialog | Qt::FramelessWindowHint)
    , m_api(api)
    , m_tray(tray)
{
    setupUI();
    StyleManager::applySavedTheme(this);
    {
        // The launcher is a QDialog (excluded from applyTheme's main-window
        // filter), but the login screen must show the same background image
        // as the apps — installed here explicitly, scaled/cropped the same
        // way. With no image set, the #launcherWindow theme gradient /
        // color tint below the label shows instead.
        const LocalSettings s = LocalSettings::load();
        StyleManager::installBackground(m_card, s.backgroundImage,
                                        s.backgroundScale, s.backgroundDim);
    }
    setupTray();
    setupNotifier();
    setupPolling();

    // At startup, the Launcher is the sole writer of the session.
    // A stale session (e.g. after a crash) is cleared.
    ActiveSessionManager::instance().clearCurrentSession();

    connect(&TranslationManager::instance(), &TranslationManager::languageChanged,
            this, &LauncherWindow::retranslateUi);
}

LauncherWindow::~LauncherWindow() = default;

void LauncherWindow::onLoginClicked()
{
    if (m_busy)
        return;

    const QString username = m_userEdit->text().trimmed();
    const QString password = m_passEdit->text();
    if (username.isEmpty() || password.isEmpty()) {
        showLoginError(tr("Please enter both username and password."));
        (username.isEmpty() ? m_userEdit : m_passEdit)->setFocus();
        return;
    }

    clearLoginError();
    setBusy(true);
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    const QJsonObject result = m_api->login(username, password);
    if (!m_api->ok()) {
        finishLogin(QString(), QString(), QString(), tr(m_api->lastError().toUtf8().constData()));
        return;
    }
    finishLogin(result.value("username").toString(), result.value("role").toString(),
                result.value("display_name").toString(), QString());
}

void LauncherWindow::finishLogin(const QString& username, const QString& role,
                                 const QString& displayName, const QString& error)
{
    setBusy(false);

    if (!error.isEmpty() || username.isEmpty()) {
        showLoginError(error.isEmpty() ? tr("Login failed") : error);
        m_passEdit->clear();
        m_passEdit->setProperty("bad", true);
        repolish(m_passEdit);
        m_passEdit->setFocus();
        return;
    }

    m_username = username;
    m_role = role;
    m_displayName = displayName;

    // Persist as the current active session (Launcher is the sole writer).
    ActiveSessionManager::SessionInfo info;
    info.username = m_username;
    info.role = m_role;
    info.displayName = m_displayName;
    info.appName = "Launcher";
    info.timestamp = QDateTime::currentDateTime().toString(Qt::ISODate);
    ActiveSessionManager::instance().setCurrentSession(info);

    showWelcomeState();
}

void LauncherWindow::onAppClicked(int index)
{
    if (!isLoggedIn()) return;
    if (index < 0 || index >= APP_COUNT) return;

    // Role gate — same rule as the window buttons (also guards the tray menu).
    if (!AccessControl::canAccessPage(APP_PAGES[index],
                                      AccessControl::stringToRole(m_role))) {
        AppHelpers::showWarning(this,
            tr("You don't have permission to open the %1 app.").arg(tr(APP_TITLES[index])));
        return;
    }

    // Update the active-session appName to whichever app we are launching.
    ActiveSessionManager::SessionInfo info = ActiveSessionManager::instance().currentSession();
    if (!info.username.isEmpty()) {
        info.appName = APP_NAMES[index];
        info.timestamp = QDateTime::currentDateTime().toString(Qt::ISODate);
        ActiveSessionManager::instance().setCurrentSession(info);
    }

    QString exePath = AppHelpers::findSiblingPath(APP_NAMES[index]);
    if (exePath.isEmpty()) {
        AppHelpers::showWarning(this,
            tr("Could not find the %1 application. Make sure it is installed.")
                .arg(tr(APP_TITLES[index])));
        return;
    }

    QStringList args = {
        "--user", m_username,
        "--role", m_role,
        "--display-name", m_displayName
    };

    if (!QProcess::startDetached(exePath, args, QCoreApplication::applicationDirPath())) {
        AppHelpers::showWarning(this, tr("Failed to launch %1.").arg(tr(APP_TITLES[index])));
        return;
    }

    // Minimize to tray once an app is open.
    hideToTray();
}

void LauncherWindow::onPollApps()
{
    if (!m_loggedIn) return;

    int running = 0;
    for (int i = 0; i < APP_COUNT; ++i) {
        if (AppLock::isRunning(APP_LOCK_FILE[i])) running++;
    }

    // Transition from "at least one app opened" to "all apps closed" means
    // the end of shift: clear the session and prompt for a new login.
    if (m_prevRunning > 0 && running == 0) {
        ActiveSessionManager::instance().clearCurrentSession();
        showLoginState();
        showNormal();
        raise();
        activateWindow();
    }

    m_prevRunning = running;
}

void LauncherWindow::onLogoutTray()
{
    if (!isLoggedIn()) return;
    if (!AppHelpers::confirmAction(this, tr("Logout? All open apps will be closed."))) return;

    doLogoutAndShowLogin();
}

void LauncherWindow::refreshLoginButton()
{
    m_loginBtn->setEnabled(!m_busy);
}

void LauncherWindow::setBusy(bool busy)
{
    m_busy = busy;
    m_userEdit->setEnabled(!busy);
    m_passEdit->setEnabled(!busy);
    m_loginBtn->setText(busy ? tr("Signing in...") : tr("Sign In"));
    if (busy) {
        m_spinAngle = 0;
        m_loginBtn->setIcon(whiteIcon(spinnerPixmap(m_spinAngle)));
        m_spinTimer->start();
    } else {
        m_spinTimer->stop();
        m_loginBtn->setIcon(whiteIcon(LineIcons::pixmap("logout", Qt::white, 18)));
    }
    refreshLoginButton();
}

void LauncherWindow::showLoginError(const QString& message)
{
    m_loginMessage->setText(message);
    m_loginMessage->show();
}

void LauncherWindow::clearLoginError()
{
    m_loginMessage->hide();
    if (m_passEdit->property("bad").toBool()) {
        m_passEdit->setProperty("bad", false);
        repolish(m_passEdit);
    }
}

void LauncherWindow::doLogoutAndShowLogin()
{
    // Grace period so apps can quit cleanly, then force-kill leftovers.
    m_notifier->notifyChange(ChangeType::SESSION_LOGOUT_REQUESTED, 0);
    QTimer::singleShot(1500, this, [this]() {
        forceKillAllApps();
        clearAppLocks();
        ActiveSessionManager::instance().clearCurrentSession();
        showLoginState();
        showNormal();
        raise();
        activateWindow();
    });
}

void LauncherWindow::onExitTray()
{
    if (!AppHelpers::confirmAction(this, tr("Exit POSBox? All open apps will be closed."))) return;
    quitEntirely();
}

// Quit everything without further confirmation.
void LauncherWindow::quitEntirely()
{
    m_notifier->notifyChange(ChangeType::SESSION_LOGOUT_REQUESTED, 0);
    QTimer::singleShot(1500, this, [this]() {
        forceKillAllApps();
        clearAppLocks();
        ActiveSessionManager::instance().clearCurrentSession();
        AppLock::clear("launcher.lock");
        QApplication::quit();
    });
}

void LauncherWindow::setupUI()
{
    setWindowTitle(tr("%1 - Launcher").arg(POSConfig::APP_NAME));
    // Targets the theme's login rules. The uploaded background image goes on
    // the card, in the constructor.
    setObjectName("launcherWindow");
    setAttribute(Qt::WA_TranslucentBackground);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(ModernDialog::ShadowMargin, ModernDialog::ShadowMargin,
                             ModernDialog::ShadowMargin, ModernDialog::ShadowMargin);
    root->setSizeConstraint(QLayout::SetFixedSize);

    auto* card = new QFrame(this);
    m_card = card;
    card->setObjectName("launcherCard");
    card->setFixedWidth(CardWidth);
    card->installEventFilter(this);  // the window is dragged by its card
    root->addWidget(card);

    auto* column = new QVBoxLayout(card);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    m_loginPage = buildLoginPage();
    m_welcomePage = buildWelcomePage();
    column->addWidget(m_loginPage);
    column->addWidget(m_welcomePage);
    column->addWidget(buildFooter());

    auto* closeBtn = new QPushButton(card);
    closeBtn->setObjectName("modalClose");
    closeBtn->setFixedSize(28, 28);
    closeBtn->setIcon(LineIcons::icon("x", LineIcons::colors().muted, 16));
    closeBtn->setIconSize(QSize(16, 16));
    closeBtn->setCursor(Qt::PointingHandCursor);
    m_closeBtn = closeBtn;
    closeBtn->setAutoDefault(false);
    closeBtn->setFocusPolicy(Qt::NoFocus);
    closeBtn->move(CardWidth - closeBtn->width() - 12, 12);
    connect(closeBtn, &QPushButton::clicked, this, &QWidget::close);

    m_spinTimer = new QTimer(this);
    m_spinTimer->setInterval(SpinnerStepMs);
    connect(m_spinTimer, &QTimer::timeout, this, [this]() {
        m_spinAngle = (m_spinAngle + 360 - 20) % 360;
        m_loginBtn->setIcon(whiteIcon(spinnerPixmap(m_spinAngle)));
    });

    connect(m_loginBtn, &QPushButton::clicked, this, &LauncherWindow::onLoginClicked);
    connect(m_passEdit, &QLineEdit::returnPressed, this, &LauncherWindow::onLoginClicked);
    connect(m_userEdit, &QLineEdit::returnPressed, this, [this]() { m_passEdit->setFocus(); });
    connect(m_userEdit, &QLineEdit::textEdited, this, &LauncherWindow::clearLoginError);
    connect(m_passEdit, &QLineEdit::textEdited, this, &LauncherWindow::clearLoginError);

    retranslateUi();
    showLoginState();
}

void LauncherWindow::retranslateUi()
{
    m_subtitle->setText(tr("Sign in to continue"));
    m_userCaption->setText(tr("Username"));
    m_userEdit->setPlaceholderText(tr("Username"));
    m_passCaption->setText(tr("Password"));
    m_passEdit->setPlaceholderText(tr("Password"));
    m_showPassAction->setToolTip(m_passEdit->echoMode() == QLineEdit::Password
                                     ? tr("Show password") : tr("Hide password"));
    m_loginBtn->setText(m_busy ? tr("Signing in...") : tr("Sign In"));
    m_appsCaption->setText(tr("Open an app"));
    for (int i = 0; i < m_appNames.size() && i < APP_COUNT; ++i)
        m_appNames[i]->setText(tr(APP_TITLES[i]));
    m_logoutBtn->setText(tr("Log out"));
    m_closeBtn->setToolTip(tr("Close"));
    m_coffeeBtn->setText(tr("Buy me a coffee"));
    m_coffeeBtn->setToolTip(tr("Enjoying POSBox? Support its development"));
    if (m_loggedIn)
        m_welcomeLabel->setText(tr("Welcome, %1").arg(m_displayName));
    if (m_trayMenu) {
        const QList<QAction*> actions = m_trayMenu->actions();
        int app = 0;
        for (QAction* act : actions) {
            if (act->isSeparator()) continue;
            const QString key = act->data().toString();
            if (key == "logout") act->setText(tr("Logout"));
            else if (key == "exit") act->setText(tr("Exit"));
            else if (app < APP_COUNT) act->setText(tr(APP_TITLES[app++]));
        }
    }
}

QWidget* LauncherWindow::buildLoginPage()
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(40, 24, 40, 20);
    layout->setSpacing(0);

    auto* logo = new QLabel(page);
    logo->setPixmap(QPixmap(LOGO_ICON).scaled(80, 80, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logo->setAlignment(Qt::AlignCenter);
    layout->addWidget(logo);
    layout->addSpacing(4);

    auto* title = new QLabel(POSConfig::APP_NAME, page);
    title->setObjectName("loginTitle");
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    m_subtitle = new QLabel(page);
    m_subtitle->setObjectName("modalSubtitle");
    m_subtitle->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_subtitle);
    layout->addSpacing(16);

    const auto addField = [page, layout](QLineEdit* edit) {
        auto* caption = new QLabel(page);
        caption->setObjectName("modalFieldLabel");
        layout->addWidget(caption);
        layout->addSpacing(5);
        layout->addWidget(edit);
        layout->addSpacing(12);
        return caption;
    };
    const LineIcons::Colors colors = LineIcons::colors();

    m_userEdit = new QLineEdit(page);
    m_userEdit->addAction(LineIcons::icon("user", colors.muted, 16), QLineEdit::LeadingPosition);
    m_userCaption = addField(m_userEdit);

    m_passEdit = new QLineEdit(page);
    m_passEdit->setEchoMode(QLineEdit::Password);
    m_showPassAction = m_passEdit->addAction(LineIcons::icon("eye", colors.muted, 18),
                                             QLineEdit::TrailingPosition);

    connect(m_showPassAction, &QAction::triggered, this, [this]() {
        const bool show = m_passEdit->echoMode() == QLineEdit::Password;
        m_passEdit->setEchoMode(show ? QLineEdit::Normal : QLineEdit::Password);
        m_showPassAction->setIcon(LineIcons::icon(show ? "eyeoff" : "eye", LineIcons::colors().muted, 18));
        m_showPassAction->setToolTip(show ? tr("Hide password") : tr("Show password"));
    });
    m_passCaption = addField(m_passEdit);

    m_loginMessage = new QLabel(page);
    m_loginMessage->setObjectName("loginMessage");
    m_loginMessage->setWordWrap(true);
    m_loginMessage->hide();
    layout->addWidget(m_loginMessage);
    layout->addSpacing(4);

    m_loginBtn = new QPushButton(page);
    m_loginBtn->setObjectName("loginBtn");
    m_loginBtn->setIcon(whiteIcon(LineIcons::pixmap("logout", Qt::white, 18)));
    m_loginBtn->setIconSize(QSize(18, 18));
    m_loginBtn->setDefault(true);
    m_loginBtn->setCursor(Qt::PointingHandCursor);
    m_loginBtn->setProperty("btnStyle", "add");
    Animations::enableGlare(m_loginBtn);
    layout->addWidget(m_loginBtn);
    return page;
}

QWidget* LauncherWindow::buildWelcomePage()
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(32, 26, 32, 16);
    layout->setSpacing(0);

    m_avatarLabel = new QLabel(page);
    m_avatarLabel->setFixedSize(68, 68);
    layout->addWidget(m_avatarLabel, 0, Qt::AlignHCenter);
    layout->addSpacing(8);

    m_welcomeLabel = new QLabel(page);
    m_welcomeLabel->setObjectName("loginWelcome");
    m_welcomeLabel->setAlignment(Qt::AlignCenter);
    m_welcomeLabel->setWordWrap(true);
    layout->addWidget(m_welcomeLabel);
    layout->addSpacing(6);

    m_roleLabel = new QLabel(page);
    m_roleLabel->setObjectName("loginRole");
    m_roleLabel->setFixedHeight(22);
    m_roleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_roleLabel, 0, Qt::AlignHCenter);
    layout->addSpacing(16);

    m_appsCaption = new QLabel(page);
    m_appsCaption->setObjectName("modalFieldLabel");
    layout->addWidget(m_appsCaption);
    layout->addSpacing(6);

    // Tiles, three on the first row and the rest centered below.
    m_appButtons.reserve(APP_COUNT);
    QHBoxLayout* row = nullptr;
    for (int i = 0; i < APP_COUNT; ++i) {
        if (i % 3 == 0) {
            if (row)
                row->addStretch(1);
            row = new QHBoxLayout;
            row->setSpacing(10);
            row->addStretch(1);
            layout->addLayout(row);
            layout->addSpacing(10);
        }
        auto* tile = new QPushButton(page);
        tile->setObjectName("launcherAppTile");
        tile->setCursor(Qt::PointingHandCursor);
        tile->setAutoDefault(false);
        tile->setEnabled(false);   // enabled/disabled looks come from the theme
        Animations::enableGlare(tile);

        auto* tileLayout = new QVBoxLayout(tile);
        tileLayout->setContentsMargins(4, 10, 4, 8);
        tileLayout->setSpacing(4);
        auto* icon = new QLabel(tile);
        icon->setPixmap(QPixmap(APP_ICONS[i]).scaled(44, 44, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        icon->setAlignment(Qt::AlignCenter);
        icon->setAttribute(Qt::WA_TransparentForMouseEvents);
        auto* name = new QLabel(tile);
        m_appNames.append(name);
        name->setObjectName("launcherAppName");
        name->setAlignment(Qt::AlignCenter);
        name->setAttribute(Qt::WA_TransparentForMouseEvents);
        tileLayout->addWidget(icon);
        tileLayout->addWidget(name);

        row->addWidget(tile);
        m_appButtons.append(tile);
        connect(tile, &QPushButton::clicked, this, [this, i]() { onAppClicked(i); });
    }
    if (row)
        row->addStretch(1);

    m_logoutBtn = new QPushButton(page);
    m_logoutBtn->setObjectName("modalQuiet");
    m_logoutBtn->setIcon(LineIcons::icon("logout", LineIcons::colors().text, 16));
    m_logoutBtn->setCursor(Qt::PointingHandCursor);
    m_logoutBtn->setAutoDefault(false);
    layout->addWidget(m_logoutBtn);
    connect(m_logoutBtn, &QPushButton::clicked, this, &LauncherWindow::onLogoutTray);
    return page;
}

QWidget* LauncherWindow::buildFooter()
{
    auto* footer = new QWidget;
    auto* outer = new QVBoxLayout(footer);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    auto* divider = new QFrame(footer);
    divider->setObjectName("modalRowLine");
    divider->setFixedHeight(1);
    outer->addWidget(divider);

    auto* layout = new QHBoxLayout;
    layout->setContentsMargins(24, 10, 18, 10);
    layout->setSpacing(8);
    outer->addLayout(layout);

    auto* version = new QLabel(QString("%1 %2").arg(POSConfig::APP_NAME, POSConfig::APP_VERSION), footer);
    version->setObjectName("launcherVersion");
    layout->addWidget(version, 1);

    // Support link: a warm amber pill with a cup, so it reads at a glance
    // as "support the developer" and never competes with Sign In.
    m_coffeeBtn = new QPushButton(footer);
    m_coffeeBtn->setObjectName("launcherCoffeeBtn");
    m_coffeeBtn->setIcon(LineIcons::icon("cup", QColor("#3B2410"), 16));
    m_coffeeBtn->setIconSize(QSize(16, 16));
    m_coffeeBtn->setCursor(Qt::PointingHandCursor);
    m_coffeeBtn->setFocusPolicy(Qt::NoFocus);
    m_coffeeBtn->setAutoDefault(false);
    Animations::enableGlare(m_coffeeBtn);
    connect(m_coffeeBtn, &QPushButton::clicked, this, []() {
        const QUrl url(QString::fromLatin1(COFFEE_URL));
#ifdef Q_OS_LINUX
        if (QProcess::startDetached("xdg-open", {url.toString()}))
            return;
        if (QProcess::startDetached("kde-open", {url.toString()}))
            return;
#endif
        QDesktopServices::openUrl(url);
    });
    layout->addWidget(m_coffeeBtn);
    return footer;
}

void LauncherWindow::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    ModernDialog::paintCardShadow(painter, rect());
}

bool LauncherWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_card) {
        if (event->type() == QEvent::MouseButtonPress
            && static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton && windowHandle()) {
            windowHandle()->startSystemMove();
        } else if (event->type() == QEvent::Resize) {
            // Keeps an uploaded background image inside the rounded card.
            if (auto* image = m_card->findChild<QLabel*>("posboxScaledBackground")) {
                QPainterPath shape;
                shape.addRoundedRect(QRectF(m_card->rect()).adjusted(1, 1, -1, -1), CardRadius, CardRadius);
                image->setMask(QRegion(shape.toFillPolygon().toPolygon()));
            }
        }
    }
    return QDialog::eventFilter(watched, event);
}

void LauncherWindow::setupTray()
{
    if (!m_tray) return;

    m_trayMenu = new QMenu(this);

    for (int i = 0; i < APP_COUNT; ++i) {
        QAction* act = m_trayMenu->addAction(QIcon(APP_ICONS[i]), tr(APP_TITLES[i]));
        connect(act, &QAction::triggered, this, [this, i]() { onAppClicked(i); });
    }

    m_trayMenu->addSeparator();
    QAction* logoutAct = m_trayMenu->addAction(tr("Logout"));
    logoutAct->setData("logout");
    connect(logoutAct, &QAction::triggered, this, &LauncherWindow::onLogoutTray);

    QAction* exitAct = m_trayMenu->addAction(tr("Exit"));
    exitAct->setData("exit");
    connect(exitAct, &QAction::triggered, this, &LauncherWindow::onExitTray);

    m_tray->setContextMenu(m_trayMenu);

    connect(m_tray, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::Trigger ||
                    reason == QSystemTrayIcon::DoubleClick) {
                    showNormal();
                    raise();
                    activateWindow();
                }
            });
    m_tray->show();
}

void LauncherWindow::setupNotifier()
{
    m_notifier = new ChangeNotifier(this);
    m_notifier->startPolling(2000);

    connect(m_notifier, &ChangeNotifier::appOpenRequested, this, &LauncherWindow::handleAppOpenRequest);
}

void LauncherWindow::setupPolling()
{
    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(1000);
    connect(m_pollTimer, &QTimer::timeout, this, &LauncherWindow::onPollApps);
    m_pollTimer->start();
}

void LauncherWindow::handleAppOpenRequest(const QString& request)
{
    // "App|flag|value" — route the request to the right app via the launcher.
    QStringList parts = request.split('|');
    if (parts.size() < 1) return;

    QString app = parts[0].trimmed();
    int index = -1;
    for (int i = 0; i < APP_COUNT; ++i) {
        if (app.compare(APP_NAMES[i], Qt::CaseInsensitive) == 0) {
            index = i;
            break;
        }
    }

    if (app.compare("Launcher", Qt::CaseInsensitive) == 0) {
        // Second-instance activation or explicit request to show the window.
        showNormal();
        raise();
        activateWindow();
        return;
    }

    if (index < 0) return;

    // Role gate — the notification click only opens apps the user may access.
    if (!AccessControl::canAccessPage(APP_PAGES[index],
                                      AccessControl::stringToRole(m_role))) {
        AppHelpers::showWarning(this,
            tr("You don't have permission to open the %1 app.").arg(tr(APP_TITLES[index])));
        return;
    }

    bool targetRunning = AppLock::isRunning(APP_LOCK_FILE[index]);

    // Build the action to forward (preserve the full "App|flag|value" deep-link).
    QString action = request;

    if (targetRunning) {
        // App is already running: forward a navigation request; the target app
        // raises its window and navigates. Never launch a second instance.
        m_notifier->notifyChange(ChangeType::APP_NAVIGATE_REQUESTED, 0, action);
        return;
    }

    hideToTray();

    QString exePath = AppHelpers::findSiblingPath(APP_NAMES[index]);
    if (exePath.isEmpty()) return;

    QStringList args;
    args << "--user" << m_username
         << "--role" << m_role
         << "--display-name" << m_displayName
         << "--action" << action;

    QProcess::startDetached(exePath, args, QCoreApplication::applicationDirPath());
}

void LauncherWindow::showLoginState()
{
    m_loggedIn = false;
    m_username.clear();
    m_role.clear();
    m_displayName.clear();

    m_userEdit->clear();
    m_passEdit->clear();
    clearLoginError();
    m_loginPage->show();
    m_welcomePage->hide();

    for (auto* btn : m_appButtons)
        btn->setEnabled(false);   // themed look follows the enabled state

    m_userEdit->setFocus();
    m_prevRunning = 0;
}

void LauncherWindow::showWelcomeState()
{
    m_loggedIn = true;

    m_welcomeLabel->setText(tr("Welcome, %1").arg(m_displayName));
    m_avatarLabel->setPixmap(avatarPixmap(m_displayName, m_avatarLabel->width()));
    m_roleLabel->setText(AppHelpers::roleName(m_role));
    m_loginPage->hide();
    m_welcomePage->show();

    // Enable app buttons per role via AccessControl; the theme styles the
    // enabled/disabled/hover states through #launcherAppBtn.
    for (int i = 0; i < APP_COUNT; ++i) {
        m_appButtons[i]->setEnabled(AccessControl::canAccessPage(
            APP_PAGES[i], AccessControl::stringToRole(m_role)));
    }
}

void LauncherWindow::hideToTray()
{
    hide();
    if (m_tray) {
        m_tray->showMessage(POSConfig::APP_NAME, tr("Running in the system tray."),
                            QSystemTrayIcon::Information, 1500);
    }
}

void LauncherWindow::forceKillAllApps()
{
    for (int i = 0; i < APP_COUNT; ++i) {
        if (!AppLock::isRunning(APP_LOCK_FILE[i]))
            continue;
        const qint64 pid = AppLock::readPid(APP_LOCK_FILE[i]);
        if (pid > 0) {
#ifdef Q_OS_WIN
            QProcess::startDetached("taskkill", QStringList() << "/F" << "/PID" << QString::number(pid));
#else
            ::kill(static_cast<pid_t>(pid), SIGTERM);
#endif
        }
    }
}

void LauncherWindow::clearAppLocks()
{
    for (int i = 0; i < APP_COUNT; ++i) {
        AppLock::clear(APP_LOCK_FILE[i]);
    }
}

void LauncherWindow::closeEvent(QCloseEvent* event)
{
    // When logged in (welcome state), closing the window only hides to tray
    // — the Launcher keeps running as the session owner. When not logged in
    // (login prompt), closing cancels the login: the Launcher ends.
    if (isLoggedIn()) {
        hide();
        event->ignore();
    } else {
        event->accept();
    }
}
