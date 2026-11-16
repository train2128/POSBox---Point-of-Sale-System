#ifndef ELIDEDLABEL_H
#define ELIDEDLABEL_H

#include <QLabel>
#include <QPainter>

// A one-line label that shortens long text with "..." instead of forcing its
// parent wider. text() still returns the full text.
class ElidedLabel : public QLabel
{
public:
    using QLabel::QLabel;

    QSize minimumSizeHint() const override
    {
        return QSize(fontMetrics().averageCharWidth() * 3, QLabel::minimumSizeHint().height());
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        const QRect area = contentsRect();
        painter.drawText(area, alignment(),
                         fontMetrics().elidedText(text(), Qt::ElideRight, area.width()));
    }
};

#endif // ELIDEDLABEL_H
