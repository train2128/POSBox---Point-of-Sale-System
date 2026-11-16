#ifndef TABLEDELEGATES_H
#define TABLEDELEGATES_H

#include <QColor>
#include <QHash>
#include <QStyledItemDelegate>

// Table cell drawn as a colored pill. Only the drawing changes: the cell's text
// stays in the model, so sorting and searching work on the real value.
class PillDelegate : public QStyledItemDelegate
{
public:
    enum class Tone { Neutral, Blue, Green, Red, Amber };

    using QStyledItemDelegate::QStyledItemDelegate;

    // Sets how a cell value looks. `label` replaces the shown text if not empty.
    void addValue(const QString& value, Tone tone, const QString& label = QString());

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;

private:
    struct Style { Tone tone; QString label; };
    QHash<QString, Style> m_styles;
};

// Table cell with a round initials badge before the name.
class AvatarDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};

// The initials badge as a picture, for use outside tables.
QPixmap avatarPixmap(const QString& name, int size);

#endif // TABLEDELEGATES_H
