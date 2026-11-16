#include "ModernDialog.h"
#include "Animations.h"
#include "LineIcons.h"
#include "Prefixes.h"
#include "StyleManager.h"
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QRegularExpressionValidator>
#include <QStyle>
#include <QVBoxLayout>
#include <QWindow>

namespace {

constexpr int CardRadius = 16;

const char* toneName(ModernDialog::Tone tone)
{
    switch (tone) {
    case ModernDialog::Tone::Green: return "green";
    case ModernDialog::Tone::Red:   return "red";
    case ModernDialog::Tone::Amber: return "amber";
    default:                        return "accent";
    }
}

void repolish(QWidget* widget)
{
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
}

} // namespace

ModernDialog::ModernDialog(Look look, const QString& icon, Tone tone, const QString& title,
                           const QString& subtitle, int width, QWidget* parent)
    : QDialog(parent, Qt::Dialog | Qt::FramelessWindowHint)
    , m_look(look)
{
    setWindowTitle(title);
    setModal(true);
    setAttribute(Qt::WA_TranslucentBackground);
    setProperty("modernDialog", true);
    setProperty("posboxCardMargin", ShadowMargin);
    // Stylesheets come from the window: one without a parent needs its own.
    if (!parent)
        StyleManager::applySavedTheme(this);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(ShadowMargin, ShadowMargin, ShadowMargin, ShadowMargin);
    root->setSizeConstraint(QLayout::SetFixedSize);

    auto* card = new QFrame(this);
    card->setObjectName("modalCard");
    root->addWidget(card);

    m_columns = new QHBoxLayout(card);
    m_columns->setContentsMargins(0, 0, 0, 0);
    m_columns->setSpacing(0);

    auto* mainColumn = new QWidget(card);
    mainColumn->setFixedWidth(width);
    m_main = new QVBoxLayout(mainColumn);
    m_main->setContentsMargins(0, 0, 0, 0);
    m_main->setSpacing(0);
    m_columns->addWidget(mainColumn);

    m_header = buildHeader(icon, tone, title, subtitle);
    m_main->addWidget(m_header);

    auto* bodyWidget = new QWidget(mainColumn);
    m_body = new QVBoxLayout(bodyWidget);
    m_body->setContentsMargins(22, look == Look::Band ? 16 : 6, 22, look == Look::Band ? 8 : 16);
    m_body->setSpacing(12);
    m_main->addWidget(bodyWidget);
}

QWidget* ModernDialog::buildHeader(const QString& icon, Tone tone, const QString& title,
                                   const QString& subtitle)
{
    auto* header = new QWidget;
    const bool band = m_look == Look::Band;
    header->setObjectName(band ? "modalBand" : "modalHeader");
    header->setProperty("tone", toneName(tone));
    header->setAttribute(Qt::WA_StyledBackground, true);
    header->installEventFilter(this);

    auto* layout = new QHBoxLayout(header);
    layout->setContentsMargins(band ? 18 : 22, 18, 16, band ? 16 : 12);
    layout->setSpacing(12);

    auto* iconLabel = new QLabel(header);
    m_headerIcon = iconLabel;
    iconLabel->setObjectName(band ? "modalIconSolid" : "modalIconDot");
    iconLabel->setProperty("tone", toneName(tone));
    iconLabel->setFixedSize(44, 44);
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setPixmap(LineIcons::pixmap(icon, band ? QColor(Qt::white) : toneColor(tone), 24));
    layout->addWidget(iconLabel, 0, Qt::AlignTop);

    auto* texts = new QVBoxLayout;
    texts->setSpacing(2);
    auto* titleLabel = new QLabel(title, header);
    titleLabel->setObjectName("modalTitle");
    m_subtitle = new QLabel(subtitle, header);
    m_subtitle->setObjectName("modalSubtitle");
    m_subtitle->setWordWrap(true);
    texts->addStretch(1);
    texts->addWidget(titleLabel);
    texts->addWidget(m_subtitle);
    texts->addStretch(1);
    layout->addLayout(texts, 1);

    auto* close = new QPushButton(header);
    close->setObjectName("modalClose");
    close->setFixedSize(28, 28);
    close->setIcon(LineIcons::icon("x", LineIcons::colors().muted, 16));
    close->setIconSize(QSize(16, 16));
    close->setCursor(Qt::PointingHandCursor);
    close->setToolTip(tr("Close"));
    close->setAutoDefault(false);
    connect(close, &QPushButton::clicked, this, &QDialog::reject);
    layout->addWidget(close, 0, Qt::AlignTop);
    return header;
}

void ModernDialog::setSubtitle(const QString& text)
{
    m_subtitle->setText(text);
}

void ModernDialog::setHeaderPicture(const QPixmap& picture)
{
    m_headerIcon->setPixmap(picture);
}

void ModernDialog::addField(const QString& label, QWidget* field)
{
    auto* column = new QVBoxLayout;
    column->setSpacing(5);
    auto* caption = new QLabel(label, this);
    caption->setObjectName("modalFieldLabel");
    column->addWidget(caption);
    column->addWidget(field);
    m_body->addLayout(column);
}

void ModernDialog::addFieldPair(const QString& label1, QWidget* field1,
                                const QString& label2, QWidget* field2)
{
    auto* row = new QHBoxLayout;
    row->setSpacing(10);
    for (const auto& [label, field] : {std::pair(label1, field1), std::pair(label2, field2)}) {
        auto* column = new QVBoxLayout;
        column->setSpacing(5);
        auto* caption = new QLabel(label, this);
        caption->setObjectName("modalFieldLabel");
        column->addWidget(caption);
        column->addWidget(field);
        row->addLayout(column, 1);
    }
    m_body->addLayout(row);
}

void ModernDialog::addRow(const QString& label, QWidget* editor)
{
    if (!m_groupLayout) {
        auto* group = new QFrame(this);
        group->setObjectName("modalGroup");
        m_groupLayout = new QVBoxLayout(group);
        m_groupLayout->setContentsMargins(1, 1, 1, 1);
        m_groupLayout->setSpacing(0);
        m_body->addWidget(group);
    } else {
        auto* divider = new QFrame(this);
        divider->setObjectName("modalRowLine");
        divider->setFixedHeight(1);
        m_groupLayout->addWidget(divider);
    }

    auto* row = new QWidget(this);
    row->setObjectName("modalRow");
    row->setAttribute(Qt::WA_StyledBackground, true);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(14, 3, 10, 3);
    layout->setSpacing(10);
    auto* caption = new QLabel(label, row);
    caption->setObjectName("modalRowLabel");
    layout->addWidget(caption);
    layout->addWidget(editor, 1);
    m_groupLayout->addWidget(row);

    editor->setProperty("modalRowEditor", true);
    editor->installEventFilter(this);
}

QVBoxLayout* ModernDialog::addSidePanel(const QString& title, int width)
{
    m_hasSide = true;
    auto* side = new QFrame(this);
    side->setObjectName("modalSide");
    side->setFixedWidth(width);
    auto* layout = new QVBoxLayout(side);
    layout->setContentsMargins(18, 20, 18, 18);
    layout->setSpacing(10);
    auto* caption = new QLabel(title.toUpper(), side);
    caption->setObjectName("modalSideTitle");
    layout->addWidget(caption);
    m_columns->addWidget(side);
    return layout;
}

QPushButton* ModernDialog::quietButton(const QString& text, const QString& icon)
{
    auto* button = new QPushButton(text, this);
    button->setObjectName("modalQuiet");
    button->setAutoDefault(false);
    button->setCursor(Qt::PointingHandCursor);
    if (!icon.isEmpty())
        button->setIcon(LineIcons::icon(icon, LineIcons::colors().text, 16));
    return button;
}

QPushButton* ModernDialog::mainButton(const QString& text, const QString& icon, const char* buttonStyle)
{
    auto* button = new QPushButton(text, this);
    button->setObjectName("modalMain");
    button->setProperty("btnStyle", buttonStyle);
    button->setCursor(Qt::PointingHandCursor);
    if (!icon.isEmpty())
        button->setIcon(LineIcons::icon(icon, Qt::white, 16));
    Animations::enableGlare(button);
    return button;
}

QHBoxLayout* ModernDialog::addCustomFooter()
{
    auto* divider = new QFrame(this);
    divider->setObjectName("modalRowLine");
    divider->setFixedHeight(1);
    m_main->addWidget(divider);

    auto* footer = new QWidget(this);
    auto* layout = new QHBoxLayout(footer);
    layout->setContentsMargins(22, 14, 22, 18);
    layout->setSpacing(10);
    m_main->addWidget(footer);
    return layout;
}

QPushButton* ModernDialog::addFooter(const QString& text, const QString& icon, const char* buttonStyle)
{
    QPushButton* main = mainButton(text, icon, buttonStyle);
    main->setDefault(true);

    if (m_look == Look::Band) {
        auto* footer = new QWidget(this);
        auto* layout = new QVBoxLayout(footer);
        layout->setContentsMargins(22, 8, 22, 12);
        layout->setSpacing(4);
        main->setProperty("modalWide", true);
        layout->addWidget(main);
        m_main->addWidget(footer);
        addCancelLink();
        return main;
    }

    QPushButton* cancel = quietButton(tr("Cancel"));
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);

    if (m_hasSide) {
        // The window is wide: the main button takes the rest of the column.
        auto* footer = new QWidget(this);
        auto* layout = new QHBoxLayout(footer);
        layout->setContentsMargins(22, 4, 22, 20);
        layout->setSpacing(10);
        layout->addWidget(cancel);
        layout->addWidget(main, 1);
        m_main->addWidget(footer);
        return main;
    }

    QHBoxLayout* layout = addCustomFooter();
    layout->addStretch(1);
    layout->addWidget(cancel);
    layout->addWidget(main);
    return main;
}

void ModernDialog::addCancelLink(const QString& text)
{
    const QString label = text.isEmpty() ? tr("Cancel") : text;
    auto* link = new QPushButton(label, this);
    link->setObjectName("modalCancelLink");
    link->setAutoDefault(false);
    link->setCursor(Qt::PointingHandCursor);
    connect(link, &QPushButton::clicked, this, &QDialog::reject);
    auto* holder = new QWidget(this);
    auto* layout = new QHBoxLayout(holder);
    layout->setContentsMargins(22, 0, 22, 12);
    layout->addStretch(1);
    layout->addWidget(link);
    layout->addStretch(1);
    m_main->addWidget(holder);
}

QFrame* ModernDialog::resultStrip(QLabel** label, QLabel** value)
{
    auto* strip = new QFrame(this);
    strip->setObjectName("modalStrip");
    auto* layout = new QHBoxLayout(strip);
    layout->setContentsMargins(14, 9, 14, 9);
    layout->setSpacing(8);
    *label = new QLabel(strip);
    (*label)->setObjectName("modalStripLabel");
    *value = new QLabel(strip);
    (*value)->setObjectName("modalStripValue");
    (*value)->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    layout->addWidget(*label, 1);
    layout->addWidget(*value);
    return strip;
}

QColor ModernDialog::toneColor(Tone tone)
{
    const bool light = StyleManager::loadColorScheme() == StyleManager::ColorScheme::Light;
    switch (tone) {
    case Tone::Green: return QColor(light ? "#219152" : "#27AE60");
    case Tone::Red:   return QColor(light ? "#D8432F" : "#E74C3C");
    case Tone::Amber: return QColor(light ? "#B8740E" : "#F0A030");
    default:          return LineIcons::colors().accent;
    }
}

QLineEdit* ModernDialog::moneyEdit(QWidget* parent, bool big)
{
    auto* edit = new QLineEdit(parent);
    edit->setObjectName(big ? "modalMoneyBig" : "modalMoney");
    // Placeholder and allowed decimals follow the shop's currency.
    const int decimals = AppConstants::PRICE_DECIMALS;
    edit->setPlaceholderText(decimals > 0 ? QString("0.%1").arg(QString(decimals, '0')) : QStringLiteral("0"));
    edit->setValidator(new QRegularExpressionValidator(
        QRegularExpression(decimals > 0 ? QString("^\\d{0,7}([.,]\\d{0,%1})?$").arg(decimals)
                                        : QStringLiteral("^\\d{0,9}$")), edit));

    auto* layout = new QHBoxLayout(edit);
    layout->setContentsMargins(0, 0, 16, 0);
    auto* suffix = new QLabel(AppConstants::CURRENCY, edit);
    suffix->setObjectName("modalMoneySuffix");
    suffix->setAttribute(Qt::WA_TransparentForMouseEvents);
    layout->addStretch(1);
    layout->addWidget(suffix);
    edit->setTextMargins(0, 0, suffix->sizeHint().width() + 16, 0);
    return edit;
}

double ModernDialog::moneyValue(const QLineEdit* edit)
{
    QString text = edit->text();
    return text.replace(',', '.').toDouble();
}

void ModernDialog::paintCardShadow(QPainter& painter, const QRect& window)
{
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    const QRectF card = QRectF(window).adjusted(ShadowMargin, ShadowMargin, -ShadowMargin, -ShadowMargin);
    const bool light = StyleManager::loadColorScheme() == StyleManager::ColorScheme::Light;
    // Rings growing outward, each adding a little darkness.
    for (int i = ShadowMargin - 2; i >= 1; --i) {
        painter.setBrush(QColor(0, 0, 0, light ? 4 : 9));
        painter.drawRoundedRect(card.adjusted(-i, -i + 3, i, i + 3), CardRadius + i, CardRadius + i);
    }
}

void ModernDialog::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    paintCardShadow(painter, rect());
}

bool ModernDialog::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_header && event->type() == QEvent::MouseButtonPress) {
        if (static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton && windowHandle())
            windowHandle()->startSystemMove();
    } else if (event->type() == QEvent::FocusIn || event->type() == QEvent::FocusOut) {
        auto* editor = qobject_cast<QWidget*>(watched);
        if (editor && editor->property("modalRowEditor").toBool() && editor->parentWidget()) {
            QWidget* row = editor->parentWidget();
            row->setProperty("focused", event->type() == QEvent::FocusIn);
            repolish(row);
        }
    }
    return QDialog::eventFilter(watched, event);
}
