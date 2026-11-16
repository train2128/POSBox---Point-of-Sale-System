#include "ToggleSwitch.h"
#include "Animations.h"
#include <QPainter>
#include <QPalette>
#include <QVariantAnimation>

namespace {
constexpr int TrackWidth = 42;
constexpr int TrackHeight = 24;
constexpr int KnobMargin = 3;
constexpr int TextGap = 8;
}

ToggleSwitch::ToggleSwitch(QWidget* parent)
    : QCheckBox(parent)
    , m_position(0.0)
    , m_slide(new QVariantAnimation(this))
{
    setCursor(Qt::PointingHandCursor);
    m_slide->setDuration(SlideMs);
    m_slide->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_slide, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        m_position = value.toDouble();
        update();
    });
    connect(this, &QCheckBox::toggled, this, [this](bool on) {
        if (!isVisible() || !Animations::enabled())
            return;
        // From wherever the knob is now, so quick double clicks stay smooth.
        const qreal from = m_slide->state() == QAbstractAnimation::Running ? m_position : (on ? 0.0 : 1.0);
        m_slide->stop();
        m_slide->setStartValue(from);
        m_slide->setEndValue(on ? 1.0 : 0.0);
        m_position = from;
        m_slide->start();
    });
}

QSize ToggleSwitch::sizeHint() const
{
    int width = TrackWidth;
    if (!text().isEmpty())
        width += TextGap + fontMetrics().horizontalAdvance(text());
    return QSize(width, qMax(TrackHeight, fontMetrics().height()));
}

QSize ToggleSwitch::minimumSizeHint() const
{
    return sizeHint();
}

bool ToggleSwitch::hitButton(const QPoint& pos) const
{
    return rect().contains(pos);
}

void ToggleSwitch::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    if (!isEnabled())
        painter.setOpacity(0.45);

    const QPalette& pal = palette();
    const QRectF track(0.5, (height() - TrackHeight) / 2.0 + 0.5, TrackWidth - 1, TrackHeight - 1);
    QColor fill = pal.color(QPalette::Mid);
    const QColor on = pal.color(QPalette::Active, QPalette::Highlight);
    // At rest the state alone decides, also when it was set with signals blocked.
    const qreal t = m_slide->state() == QAbstractAnimation::Running ? m_position : (isChecked() ? 1.0 : 0.0);
    fill = QColor::fromRgbF(fill.redF() + (on.redF() - fill.redF()) * t,
                            fill.greenF() + (on.greenF() - fill.greenF()) * t,
                            fill.blueF() + (on.blueF() - fill.blueF()) * t);
    painter.setPen(Qt::NoPen);
    painter.setBrush(fill);
    painter.drawRoundedRect(track, track.height() / 2, track.height() / 2);

    const qreal knob = TrackHeight - 2 * KnobMargin;
    const qreal x = KnobMargin + (TrackWidth - 2 * KnobMargin - knob) * t;
    painter.setBrush(Qt::white);
    painter.drawEllipse(QRectF(x, track.top() - 0.5 + KnobMargin, knob, knob));

    if (!text().isEmpty()) {
        painter.setPen(pal.color(QPalette::WindowText));
        painter.drawText(rect().adjusted(TrackWidth + TextGap, 0, 0, 0),
                         Qt::AlignLeft | Qt::AlignVCenter, text());
    }
}
