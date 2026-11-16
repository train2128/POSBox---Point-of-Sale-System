#ifndef TOGGLESWITCH_H
#define TOGGLESWITCH_H

#include <QCheckBox>

class QVariantAnimation;

// A QCheckBox drawn as a sliding on/off switch. It behaves exactly like a
// checkbox (isChecked, setChecked, toggled, setEnabled).
class ToggleSwitch : public QCheckBox
{
public:
    static constexpr int SlideMs = 220;

    explicit ToggleSwitch(QWidget* parent = nullptr);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    bool hitButton(const QPoint& pos) const override;

private:
    qreal m_position;  // knob while sliding: 0 = off, 1 = on
    QVariantAnimation* m_slide;
};

#endif // TOGGLESWITCH_H
