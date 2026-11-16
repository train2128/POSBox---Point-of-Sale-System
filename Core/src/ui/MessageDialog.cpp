#include "MessageDialog.h"
#include "Animations.h"
#include "LineIcons.h"
#include "ModernDialog.h"
#include "StyleManager.h"
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWindow>

namespace {

constexpr int CardWidth = 340;

ModernDialog::Tone toneFor(MessageDialog::Kind kind)
{
    switch (kind) {
    case MessageDialog::Kind::Success: return ModernDialog::Tone::Green;
    case MessageDialog::Kind::Warning: return ModernDialog::Tone::Amber;
    case MessageDialog::Kind::Question: return ModernDialog::Tone::Accent;
    default: return ModernDialog::Tone::Red;
    }
}

const char* toneName(MessageDialog::Kind kind)
{
    switch (kind) {
    case MessageDialog::Kind::Success: return "green";
    case MessageDialog::Kind::Warning: return "amber";
    case MessageDialog::Kind::Question: return "accent";
    default: return "red";
    }
}

const char* iconName(MessageDialog::Kind kind)
{
    switch (kind) {
    case MessageDialog::Kind::Success: return "check";
    case MessageDialog::Kind::Warning: return "alert";
    case MessageDialog::Kind::Question: return "question";
    case MessageDialog::Kind::Danger: return "trash";
    default: return "x";
    }
}

} // namespace

MessageDialog::MessageDialog(Kind kind, const QString& title, const QString& text, QWidget* parent)
    : QDialog(parent, Qt::Dialog | Qt::FramelessWindowHint)
    , m_kind(kind)
{
    setWindowTitle(title);
    setObjectName("messageDialog");
    setModal(true);
    setAttribute(Qt::WA_TranslucentBackground);
    setProperty("modernDialog", true);
    setProperty("posboxCardMargin", ModernDialog::ShadowMargin);
    // Stylesheets come from the window: one without a parent needs its own.
    if (!parent)
        StyleManager::applySavedTheme(this);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(ModernDialog::ShadowMargin, ModernDialog::ShadowMargin,
                             ModernDialog::ShadowMargin, ModernDialog::ShadowMargin);
    root->setSizeConstraint(QLayout::SetFixedSize);

    auto* card = new QFrame(this);
    card->setObjectName("modalCard");
    card->setFixedWidth(CardWidth);
    card->installEventFilter(this);
    m_card = card;
    root->addWidget(card);

    m_column = new QVBoxLayout(card);
    m_column->setContentsMargins(24, 22, 24, 22);
    m_column->setSpacing(0);

    auto* icon = new QLabel(card);
    icon->setObjectName("messageIcon");
    icon->setProperty("tone", toneName(kind));
    icon->setFixedSize(60, 60);
    icon->setAlignment(Qt::AlignCenter);
    icon->setPixmap(LineIcons::pixmap(iconName(kind), ModernDialog::toneColor(toneFor(kind)), 32));
    m_column->addWidget(icon, 0, Qt::AlignHCenter);
    m_column->addSpacing(10);

    auto* titleLabel = new QLabel(title, card);
    titleLabel->setObjectName("messageTitle");
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setWordWrap(true);
    titleLabel->setTextFormat(Qt::PlainText);
    m_column->addWidget(titleLabel);

    if (!text.isEmpty()) {
        m_column->addSpacing(4);
        auto* textLabel = new QLabel(text, card);
        textLabel->setObjectName("messageText");
        textLabel->setAlignment(Qt::AlignCenter);
        textLabel->setWordWrap(true);
        textLabel->setTextFormat(Qt::PlainText);
        m_column->addWidget(textLabel);
    }
    m_column->addSpacing(18);
    setButtons(tr("OK"));
}

void MessageDialog::setAmount(const QString& amount)
{
    if (!m_amount) {
        m_amount = new QLabel(m_card);
        m_amount->setObjectName("messageAmount");
        m_amount->setAlignment(Qt::AlignCenter);
        // Above the buttons and the gap over them.
        m_column->insertWidget(m_column->count() - 2, m_amount);
    }
    m_amount->setText(amount);
}

void MessageDialog::setButtons(const QString& mainText, const QString& quietText, const QString& mainIcon)
{
    delete m_buttons;
    m_buttons = new QWidget(m_card);
    auto* row = new QHBoxLayout(m_buttons);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(10);

    if (!quietText.isEmpty()) {
        auto* quiet = new QPushButton(quietText, m_buttons);
        quiet->setObjectName("modalQuiet");
        quiet->setAutoDefault(false);
        quiet->setCursor(Qt::PointingHandCursor);
        connect(quiet, &QPushButton::clicked, this, &QDialog::reject);
        row->addWidget(quiet, 1);
    }

    auto* main = new QPushButton(mainText, m_buttons);
    main->setObjectName("modalMain");
    main->setProperty("btnStyle", m_kind == Kind::Danger ? "delete" : "edit");
    main->setCursor(Qt::PointingHandCursor);
    main->setDefault(true);
    if (!mainIcon.isEmpty())
        main->setIcon(LineIcons::icon(mainIcon, Qt::white, 16));
    Animations::enableGlare(main);
    connect(main, &QPushButton::clicked, this, &QDialog::accept);
    row->addWidget(main, 1);
    m_column->addWidget(m_buttons);
    main->setFocus();
}

void MessageDialog::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    ModernDialog::paintCardShadow(painter, rect());
}

bool MessageDialog::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_card && event->type() == QEvent::MouseButtonPress
        && static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton && windowHandle()) {
        windowHandle()->startSystemMove();
    }
    return QDialog::eventFilter(watched, event);
}
