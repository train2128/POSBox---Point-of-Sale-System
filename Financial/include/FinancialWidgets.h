#ifndef FINANCIALWIDGETS_H
#define FINANCIALWIDGETS_H

#include <QImage>
#include <QList>
#include <QPair>
#include <QStyledItemDelegate>
#include <QVector>
#include <QWidget>

// Income and expense bars for each day of a week, with the amounts under them.
class WeekChart : public QWidget
{
public:
    using QWidget::QWidget;

    void setDays(const QVector<double>& income, const QVector<double>& expenses, int todayIndex);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QVector<double> m_income;
    QVector<double> m_expenses;
    int m_today = -1;
};

// The best-selling products as labelled horizontal bars.
class TopSellersChart : public QWidget
{
public:
    using QWidget::QWidget;

    void setItems(const QList<QPair<QString, double>>& items);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QList<QPair<QString, double>> m_items;
};

// Income for each month of a year. Clicking a month selects it.
class YearChart : public QWidget
{
    Q_OBJECT

public:
    using QWidget::QWidget;

    // `lastFinishedOrCurrent`: months after this one (1-12) are drawn as still to come.
    void setMonths(const QVector<double>& income, int lastFinishedOrCurrent);
    void setSelectedMonth(int month);  // 1-12, 0 for none

signals:
    void monthClicked(int month);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    QVector<double> m_income;
    int m_lastMonth = 12;
    int m_selected = 0;
};


// A table cell with a second, quieter line under its text (from SecondLineRole).
class TwoLineDelegate : public QStyledItemDelegate
{
public:
    static constexpr int SecondLineRole = Qt::UserRole + 1;

    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
};


#endif // FINANCIALWIDGETS_H
