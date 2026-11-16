#include "FinancialWidgets.h"
#include "LineIcons.h"
#include "Prefixes.h"
#include "StyleManager.h"
#include "TranslationManager.h"
#include <QCoreApplication>
#include <QApplication>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

namespace {

const QColor IncomeColor("#27AE60");
const QColor ExpenseColor("#E74C3C");
const QColor AccentColor("#4A90E2");

bool isLightTheme()
{
    return StyleManager::loadColorScheme() == StyleManager::ColorScheme::Light;
}

QColor faintColor()
{
    return isLightTheme() ? QColor(120, 90, 50, 26) : QColor(255, 255, 255, 18);
}

QColor futureColor()
{
    return isLightTheme() ? QColor(120, 90, 50, 60) : QColor(255, 255, 255, 40);
}

// Short enough for a narrow column: large amounts drop their decimals.
QString compactAmount(double value)
{
    if (qAbs(value) >= 1000.0)
        return QLocale(QLocale::English).toString(value, 'f', 0);
    return AppHelpers::formatAmount(value);
}

void setPixelFont(QPainter& painter, int pixelSize, bool bold)
{
    QFont font = painter.font();
    font.setPixelSize(pixelSize);
    font.setBold(bold);
    painter.setFont(font);
}

void drawBar(QPainter& painter, const QRectF& rect, const QColor& color, qreal radius)
{
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawRoundedRect(rect, radius, radius);
}

} // namespace

// ── WeekChart ───────────────────────────────────────────────────────────────

void WeekChart::setDays(const QVector<double>& income, const QVector<double>& expenses, int todayIndex)
{
    m_income = income;
    m_expenses = expenses;
    m_today = todayIndex;
    update();
}

void WeekChart::paintEvent(QPaintEvent*)
{
    constexpr int TextArea = 54;
    constexpr int BarWidth = 12;
    // Sunday first, short names in the app language.
    const QLocale locale = TranslationManager::instance().locale();
    QStringList names;
    for (int day : {7, 1, 2, 3, 4, 5, 6})
        names << locale.dayName(day, QLocale::ShortFormat);

    if (m_income.size() != 7 || m_expenses.size() != 7)
        return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const LineIcons::Colors colors = LineIcons::colors();

    double largest = 1.0;
    for (int day = 0; day < 7; ++day)
        largest = qMax(largest, qMax(m_income[day], m_expenses[day]));

    const qreal column = width() / 7.0;
    const qreal barsHeight = qMax(10, height() - TextArea - 8);
    const qreal baseline = 6 + barsHeight;

    for (int day = 0; day < 7; ++day) {
        const qreal left = day * column;
        if (day == m_today)
            drawBar(painter, QRectF(left + 2, 0, column - 4, height()), faintColor(), 8);

        const qreal center = left + column / 2;
        const qreal incomeHeight = qMax(1.5, m_income[day] / largest * barsHeight);
        const qreal expenseHeight = qMax(1.5, m_expenses[day] / largest * barsHeight);
        drawBar(painter, QRectF(center - BarWidth - 1, baseline - incomeHeight, BarWidth, incomeHeight),
                IncomeColor, 3);
        drawBar(painter, QRectF(center + 1, baseline - expenseHeight, BarWidth, expenseHeight),
                ExpenseColor, 3);

        const bool today = day == m_today;
        setPixelFont(painter, 11, today);
        painter.setPen(today ? colors.text : colors.muted);
        painter.drawText(QRectF(left, baseline + 3, column, 16), Qt::AlignCenter, names[day]);
        setPixelFont(painter, 10, true);
        painter.setPen(IncomeColor);
        painter.drawText(QRectF(left, baseline + 19, column, 15), Qt::AlignCenter,
                         compactAmount(m_income[day]));
        setPixelFont(painter, 10, false);
        painter.setPen(colors.muted);
        painter.drawText(QRectF(left, baseline + 33, column, 15), Qt::AlignCenter,
                         compactAmount(m_expenses[day]));
    }
}

// ── TopSellersChart ─────────────────────────────────────────────────────────

void TopSellersChart::setItems(const QList<QPair<QString, double>>& items)
{
    m_items = items;
    update();
}

void TopSellersChart::paintEvent(QPaintEvent*)
{
    constexpr int Rows = 3;
    constexpr int NameShare = 3;  // a third of the width
    constexpr int QuantityWidth = 40;
    constexpr int BarHeight = 12;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const LineIcons::Colors colors = LineIcons::colors();

    if (m_items.isEmpty()) {
        painter.setPen(colors.muted);
        painter.drawText(rect(), Qt::AlignCenter,
                         QCoreApplication::translate("FinancialWindow", "No sales this month"));
        return;
    }

    double largest = 1.0;
    for (const auto& item : m_items)
        largest = qMax(largest, item.second);

    const qreal rowHeight = qreal(height()) / Rows;
    const int nameWidth = width() / NameShare;
    const qreal barLeft = nameWidth + 4;
    const qreal barWidth = qMax(10, width() - nameWidth - QuantityWidth - 8);

    for (int row = 0; row < m_items.size() && row < Rows; ++row) {
        const qreal top = row * rowHeight;
        const QRectF nameArea(0, top, nameWidth, rowHeight);
        setPixelFont(painter, 13, false);
        painter.setPen(colors.text);
        painter.drawText(nameArea, Qt::AlignLeft | Qt::AlignVCenter,
                         painter.fontMetrics().elidedText(m_items[row].first, Qt::ElideRight, nameWidth));

        const qreal barTop = top + (rowHeight - BarHeight) / 2;
        drawBar(painter, QRectF(barLeft, barTop, barWidth, BarHeight), faintColor(), BarHeight / 2.0);
        drawBar(painter, QRectF(barLeft, barTop, qMax(qreal(BarHeight), barWidth * m_items[row].second / largest),
                                BarHeight), AccentColor, BarHeight / 2.0);

        setPixelFont(painter, 13, true);
        painter.setPen(colors.text);
        painter.drawText(QRectF(width() - QuantityWidth, top, QuantityWidth, rowHeight),
                         Qt::AlignRight | Qt::AlignVCenter, QString::number(m_items[row].second, 'f', 0));
    }
}

// ── YearChart ───────────────────────────────────────────────────────────────

void YearChart::setMonths(const QVector<double>& income, int lastFinishedOrCurrent)
{
    m_income = income;
    m_lastMonth = lastFinishedOrCurrent;
    update();
}

void YearChart::setSelectedMonth(int month)
{
    m_selected = month;
    update();
}

void YearChart::mousePressEvent(QMouseEvent* event)
{
    const int month = int(event->position().x() / (width() / 12.0)) + 1;
    if (month >= 1 && month <= 12)
        emit monthClicked(month);
}

void YearChart::paintEvent(QPaintEvent*)
{
    constexpr int TextArea = 34;
    const QLocale locale = TranslationManager::instance().locale();
    QStringList names;
    for (int month = 1; month <= 12; ++month)
        names << locale.standaloneMonthName(month, QLocale::ShortFormat);
    if (m_income.size() != 12)
        return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const LineIcons::Colors colors = LineIcons::colors();

    double largest = 1.0;
    for (const double value : m_income)
        largest = qMax(largest, value);

    const qreal column = width() / 12.0;
    const qreal barWidth = qMin(32.0, column * 0.6);
    const qreal barsHeight = qMax(10, height() - TextArea - 4);
    const qreal baseline = 4 + barsHeight;

    for (int month = 0; month < 12; ++month) {
        const qreal left = month * column;
        const bool selected = month + 1 == m_selected;
        const bool future = month + 1 > m_lastMonth;
        if (selected)
            drawBar(painter, QRectF(left + 2, 0, column - 4, height()), faintColor(), 8);

        const qreal barHeight = qMax(2.0, m_income[month] / largest * barsHeight);
        drawBar(painter, QRectF(left + (column - barWidth) / 2, baseline - barHeight, barWidth, barHeight),
                selected ? AccentColor : future ? futureColor() : IncomeColor, 5);

        setPixelFont(painter, 11, selected);
        painter.setPen(selected ? colors.text : colors.muted);
        painter.drawText(QRectF(left, baseline + 2, column, 16), Qt::AlignCenter, names[month]);
        setPixelFont(painter, 10, false);
        painter.setPen(colors.muted);
        painter.drawText(QRectF(left, baseline + 17, column, 14), Qt::AlignCenter,
                         QLocale(QLocale::English).toString(m_income[month], 'f', 0));
    }
}

// ── Delegates ───────────────────────────────────────────────────────────────

void TwoLineDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                            const QModelIndex& index) const
{
    constexpr int SidePadding = 8;

    QStyleOptionViewItem base = option;
    initStyleOption(&base, index);
    const QString title = base.text;
    base.text.clear();
    QApplication::style()->drawControl(QStyle::CE_ItemViewItem, &base, painter, base.widget);

    const QRect area = option.rect.adjusted(SidePadding, 0, -SidePadding, 0);
    const int half = area.height() / 2;
    const LineIcons::Colors colors = LineIcons::colors();

    painter->save();
    QFont titleFont = option.font;
    titleFont.setWeight(QFont::DemiBold);
    painter->setFont(titleFont);
    painter->setPen(colors.text);
    painter->drawText(QRect(area.x(), area.y(), area.width(), half), Qt::AlignLeft | Qt::AlignBottom,
                      QFontMetrics(titleFont).elidedText(title, Qt::ElideRight, area.width()));
    QFont subFont = option.font;
    subFont.setPixelSize(11);
    painter->setFont(subFont);
    painter->setPen(colors.muted);
    painter->drawText(QRect(area.x(), area.y() + half, area.width(), area.height() - half),
                      Qt::AlignLeft | Qt::AlignTop, index.data(SecondLineRole).toString());
    painter->restore();
}

