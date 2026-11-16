#include "ComboStyle.h"
#include "LineIcons.h"
#include <QAbstractItemView>
#include <QComboBox>
#include <QPainter>
#include <QStyledItemDelegate>

namespace {

constexpr int RowExtraHeight = 16;
constexpr int DotSize = 10;
constexpr int TextIndent = 30;

class ComboRowDelegate final : public QStyledItemDelegate
{
public:
    explicit ComboRowDelegate(QComboBox* combo)
        : QStyledItemDelegate(combo), m_combo(combo) {}

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        const QSize base = QStyledItemDelegate::sizeHint(option, index);
        return QSize(base.width() + TextIndent + 12, option.fontMetrics.height() + RowExtraHeight);
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        const LineIcons::Colors colors = LineIcons::colors();
        const bool chosen = index.row() == m_combo->currentIndex();
        const bool highlighted = option.state & (QStyle::State_Selected | QStyle::State_MouseOver);
        const QRectF row = QRectF(option.rect).adjusted(0, 1, 0, -1);
        const qreal radius = row.height() / 2;

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(Qt::NoPen);
        if (chosen) {
            QColor tint = colors.accent;
            tint.setAlphaF(0.20);
            painter->setBrush(tint);
            painter->drawRoundedRect(row, radius, radius);
            painter->setBrush(colors.accent);
            painter->drawEllipse(QRectF(row.left() + 10, row.center().y() - DotSize / 2.0, DotSize, DotSize));
        } else if (highlighted) {
            QColor tint = colors.text;
            tint.setAlphaF(0.09);
            painter->setBrush(tint);
            painter->drawRoundedRect(row, radius, radius);
        }

        QFont font = option.font;
        font.setBold(chosen);
        painter->setFont(font);
        painter->setPen(chosen ? colors.accent : colors.text);
        const QRectF textRect = row.adjusted(TextIndent, 0, -8, 0);
        const QString text = QFontMetrics(font).elidedText(
            index.data(Qt::DisplayRole).toString(), Qt::ElideRight, int(textRect.width()));
        painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, text);
        painter->restore();
    }

private:
    QComboBox* m_combo;
};

} // namespace

namespace ComboStyle {

void prepare(QComboBox* combo)
{
    combo->setItemDelegate(new ComboRowDelegate(combo));
    combo->view()->setMouseTracking(true);
    QWidget* popup = combo->view()->window();
    popup->setWindowFlags(popup->windowFlags() | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    popup->setAttribute(Qt::WA_TranslucentBackground);
}

} // namespace ComboStyle
