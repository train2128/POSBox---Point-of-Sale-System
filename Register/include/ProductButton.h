#ifndef PRODUCTBUTTON_H
#define PRODUCTBUTTON_H

#include <QFrame>
#include <QMouseEvent>

/**
 * Clickable product button widget based on QFrame.
 * Emits clicked() on mouse press, supports QSS :hover via WA_Hover.
 */
class ProductButton : public QFrame
{
    Q_OBJECT

public:
    explicit ProductButton(QWidget* parent = nullptr);

signals:
    void clicked();

protected:
    void mousePressEvent(QMouseEvent* event) override;
};

#endif // PRODUCTBUTTON_H
