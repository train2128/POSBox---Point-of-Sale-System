#include "TableDelegates.h"
#include "StyleManager.h"
#include <QApplication>
#include <QPainter>
#include <QPixmap>
#include <QStyle>

namespace {

struct ToneColors { QColor text; QColor background; };

ToneColors toneColors(PillDelegate::Tone tone)
{
    const bool light = StyleManager::loadColorScheme() == StyleManager::ColorScheme::Light;
    switch (tone) {
    case PillDelegate::Tone::Blue:
        return light ? ToneColors{QColor("#A86F36"), QColor("#F1DDC2")}
                     : ToneColors{QColor("#6AA8F0"), QColor("#23395A")};
    case PillDelegate::Tone::Green:
        return light ? ToneColors{QColor("#1E8E3E"), QColor("#DCEBD8")}
                     : ToneColors{QColor("#2ECC71"), QColor("#1B3A2F")};
    case PillDelegate::Tone::Red:
        return light ? ToneColors{QColor("#C0392B"), QColor("#F5D9CF")}
                     : ToneColors{QColor("#F0604F"), QColor("#43282C")};
    case PillDelegate::Tone::Amber:
        return light ? ToneColors{QColor("#A8720E"), QColor("#F4E4BE")}
                     : ToneColors{QColor("#F2B23E"), QColor("#3F3524")};
    case PillDelegate::Tone::Neutral:
        break;
    }
    return light ? ToneColors{QColor("#6E6049"), QColor("#EFE3CC")}
                 : ToneColors{QColor("#AEB8C6"), QColor("#2F3A4D")};
}

QString initialsOf(const QString& name)
{
    const QStringList parts = name.split(' ', Qt::SkipEmptyParts);
    if (parts.isEmpty())
        return QStringLiteral("?");
    QString result = parts.first().left(1);
    if (parts.size() > 1)
        result += parts.last().left(1);
    return result.toUpper();
}

QColor avatarColor(const QString& name)
{
    static const QList<QColor> Colors{QColor("#4A90E2"), QColor("#2EAD78"), QColor("#8E6BD8"),
                                      QColor("#D9835B"), QColor("#C2558A"), QColor("#3AA6B9")};
    int hash = 0;
    for (const QChar c : name)
        hash = hash * 31 + c.unicode();
    return Colors[qAbs(hash) % Colors.size()];
}

void drawAvatar(QPainter* painter, const QRect& rect, const QString& name)
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(Qt::NoPen);
    painter->setBrush(avatarColor(name));
    painter->drawEllipse(rect);
    QFont font = painter->font();
    font.setPixelSize(qRound(rect.height() * 0.38));
    font.setBold(true);
    painter->setFont(font);
    painter->setPen(Qt::white);
    painter->drawText(rect, Qt::AlignCenter, initialsOf(name));
    painter->restore();
}

// The row background (selection, hover) without the cell's own text.
void drawCellBackground(QPainter* painter, QStyleOptionViewItem background)
{
    background.text.clear();
    const QWidget* widget = background.widget;
    QStyle* style = widget ? widget->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &background, painter, widget);
}

} // namespace

void PillDelegate::addValue(const QString& value, Tone tone, const QString& label)
{
    m_styles.insert(value, Style{tone, label});
}

void PillDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                         const QModelIndex& index) const
{
    QStyleOptionViewItem cell = option;
    initStyleOption(&cell, index);
    drawCellBackground(painter, cell);

    const QString value = index.data(Qt::DisplayRole).toString();
    if (value.isEmpty())
        return;
    const auto found = m_styles.constFind(value);
    const Tone tone = found != m_styles.constEnd() ? found->tone : Tone::Neutral;
    const QString label = found != m_styles.constEnd() && !found->label.isEmpty() ? found->label : value;
    const ToneColors colors = toneColors(tone);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    QFont font = option.font;
    font.setBold(true);
    if (font.pixelSize() > 0)
        font.setPixelSize(qMax(10, qRound(font.pixelSize() * 0.88)));
    else
        font.setPointSizeF(font.pointSizeF() * 0.88);
    painter->setFont(font);
    const QFontMetrics metrics(font);
    const int width = qMin(metrics.horizontalAdvance(label) + 22, option.rect.width() - 4);
    const int height = metrics.height() + 8;
    const QRect pill(option.rect.x() + (option.rect.width() - width) / 2,
                     option.rect.y() + (option.rect.height() - height) / 2, width, height);
    painter->setPen(Qt::NoPen);
    painter->setBrush(colors.background);
    painter->drawRoundedRect(pill, height / 2.0, height / 2.0);
    painter->setPen(colors.text);
    painter->drawText(pill, Qt::AlignCenter, label);
    painter->restore();
}

void AvatarDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                           const QModelIndex& index) const
{
    QStyleOptionViewItem cell = option;
    initStyleOption(&cell, index);
    drawCellBackground(painter, cell);

    const QString name = index.data(Qt::DisplayRole).toString();
    if (name.isEmpty())
        return;
    const int size = qMin(28, option.rect.height() - 8);
    const QRect badge(option.rect.x() + 8, option.rect.y() + (option.rect.height() - size) / 2, size, size);
    drawAvatar(painter, badge, name);

    painter->save();
    QColor textColor = option.palette.color(QPalette::Text);
    painter->setPen(textColor);
    const QRect textRect = option.rect.adjusted(size + 16, 0, -4, 0);
    painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter,
                      option.fontMetrics.elidedText(name, Qt::ElideRight, textRect.width()));
    painter->restore();
}

QSize AvatarDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QSize size = QStyledItemDelegate::sizeHint(option, index);
    size.setWidth(size.width() + 44);
    return size;
}

QPixmap avatarPixmap(const QString& name, int size)
{
    constexpr qreal Scale = 3.0;
    QPixmap px(QSize(size, size) * Scale);
    px.fill(Qt::transparent);
    QPainter painter(&px);
    drawAvatar(&painter, px.rect(), name);
    painter.end();
    px.setDevicePixelRatio(Scale);
    return px;
}
