#include "FinancialWindow.h"
#include "ui_financialwindow.h"
#include "FinancialConstants.h"
#include "FinancialUIMap.h"
#include "FinancialWidgets.h"
#include "LineIcons.h"
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {

constexpr int ExpenseRowHeight = 50;
constexpr int YearChartHeight = 136;

QLabel* roleLabel(const QString& text, const char* role)
{
    auto* label = new QLabel(text);
    label->setProperty("uiRole", role);
    return label;
}

QFrame* makeCard(QVBoxLayout*& column, int spacing = 8)
{
    auto* card = new QFrame;
    card->setProperty("uiCard", true);
    column = new QVBoxLayout(card);
    column->setContentsMargins(14, 10, 14, 12);
    column->setSpacing(spacing);
    return card;
}

void usePlainStyle(QTableWidget* table)
{
    table->setProperty("plainTable", true);
    table->setFrameShape(QFrame::NoFrame);
    table->setShowGrid(false);
    table->setAlternatingRowColors(false);
    table->setFocusPolicy(Qt::NoFocus);
    table->verticalHeader()->setVisible(false);
    table->style()->unpolish(table);
    table->style()->polish(table);
}

// Removes a page's designer layout. Its widgets stay alive for the new layout.
void clearPageLayout(QWidget* page)
{
    delete page->layout();
}

} // namespace

// One of the summary numbers at the top of the Overview.
void FinancialWindow::addKpiCard(QHBoxLayout* row, const char* tone, Kpi& kpi)
{
    kpi.card = new QFrame;
    kpi.card->setObjectName("kpiCard");
    kpi.card->setProperty("tone", tone);
    auto* cardRow = new QHBoxLayout(kpi.card);
    cardRow->setContentsMargins(4, 12, 12, 10);
    cardRow->setSpacing(10);

    kpi.bar = new QFrame;
    kpi.bar->setObjectName("kpiBar");
    kpi.bar->setProperty("tone", tone);
    kpi.bar->setFixedWidth(3);
    cardRow->addWidget(kpi.bar);

    auto* text = new QVBoxLayout;
    text->setSpacing(0);
    kpi.title = new QLabel;
    kpi.title->setObjectName("kpiCaption");
    text->addWidget(kpi.title);
    kpi.value = new QLabel("—");
    kpi.value->setObjectName("kpiValue");
    kpi.value->setProperty("tone", tone);
    text->addWidget(kpi.value);
    kpi.sub = new QLabel;
    kpi.sub->setObjectName("kpiSub");
    text->addWidget(kpi.sub);
    cardRow->addLayout(text, 1);

    row->addWidget(kpi.card, 1);
}

// Arranges the two tabs. Every control comes from the designer file and keeps
// its signals; only its place on screen changes.
void FinancialWindow::buildLayout()
{
    FinancialUIMap::userLabel->setProperty("uiRole", "plain");

    // ── Overview ────────────────────────────────────────────────────────────
    clearPageLayout(ui->Tab1);
    ui->expensesBox->hide();

    auto* overview = new QVBoxLayout(ui->Tab1);
    overview->setContentsMargins(10, 10, 10, 10);
    overview->setSpacing(10);

    auto* periodRow = new QHBoxLayout;
    periodRow->setSpacing(6);
    m_periodTitle = roleLabel(QString(), "sectionTitle");
    periodRow->addWidget(m_periodTitle, 1);
    FinancialUIMap::monthSelect->setMinimumWidth(170);
    FinancialUIMap::yearSelect->setMinimumWidth(120);
    periodRow->addWidget(FinancialUIMap::monthSelect);
    periodRow->addWidget(FinancialUIMap::yearSelect);
    overview->addLayout(periodRow);

    auto* kpis = new QHBoxLayout;
    kpis->setSpacing(9);
    addKpiCard(kpis, "good", m_kpiIncome);
    addKpiCard(kpis, "neutral", m_kpiCost);
    addKpiCard(kpis, "accent", m_kpiBenefit);
    addKpiCard(kpis, "warn", m_kpiExpenses);
    addKpiCard(kpis, "good", m_kpiRest);
    overview->addLayout(kpis);

    QVBoxLayout* weekColumn = nullptr;
    QFrame* weekCard = makeCard(weekColumn, 4);
    auto* weekHeader = new QHBoxLayout;
    m_weekTitle = roleLabel(QString(), "cardTitle");
    weekHeader->addWidget(m_weekTitle, 1);
    m_weekLegend = new QLabel;
    m_weekLegend->setObjectName("kpiSub");
    weekHeader->addWidget(m_weekLegend);
    weekColumn->addLayout(weekHeader);
    m_weekChart = new WeekChart;
    weekColumn->addWidget(m_weekChart, 1);

    QVBoxLayout* topColumn = nullptr;
    QFrame* topCard = makeCard(topColumn, 4);
    m_topTitle = roleLabel(QString(), "cardTitle");
    topColumn->addWidget(m_topTitle);
    m_topSellers = new TopSellersChart;
    topColumn->addWidget(m_topSellers, 1);

    auto* leftColumn = new QVBoxLayout;
    leftColumn->setSpacing(10);
    leftColumn->addWidget(weekCard, 5);
    leftColumn->addWidget(topCard, 3);

    QVBoxLayout* expenseColumn = nullptr;
    QFrame* expenseCard = makeCard(expenseColumn);
    m_expenseTitle = roleLabel(QString(), "cardTitle");
    expenseColumn->addWidget(m_expenseTitle);
    FinancialUIMap::addExpenseBtn->setProperty("compact", true);
    FinancialUIMap::deleteExpenseBtn->setProperty("compact", true);
    FinancialUIMap::addExpenseBtn->setIcon(LineIcons::icon("plus", Qt::white, 16));

    QTableWidget* expenses = FinancialUIMap::expensesTable;
    usePlainStyle(expenses);
    expenses->setColumnCount(2);
    expenses->setHorizontalHeaderItem(FinancialConstants::COL_EXP_NAME, new QTableWidgetItem);
    expenses->setHorizontalHeaderItem(FinancialConstants::COL_EXP_AMOUNT, new QTableWidgetItem);
    expenses->horizontalHeaderItem(FinancialConstants::COL_EXP_NAME)->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    expenses->horizontalHeaderItem(FinancialConstants::COL_EXP_AMOUNT)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    expenses->horizontalHeader()->setSectionResizeMode(FinancialConstants::COL_EXP_NAME, QHeaderView::Stretch);
    expenses->horizontalHeader()->setSectionResizeMode(FinancialConstants::COL_EXP_AMOUNT, QHeaderView::ResizeToContents);
    expenses->verticalHeader()->setDefaultSectionSize(ExpenseRowHeight);
    expenses->setItemDelegateForColumn(FinancialConstants::COL_EXP_NAME, new TwoLineDelegate(expenses));
    expenseColumn->addWidget(expenses, 1);

    auto* expenseFooter = new QHBoxLayout;
    expenseFooter->setSpacing(6);
    expenseFooter->addWidget(FinancialUIMap::addExpenseBtn);
    expenseFooter->addWidget(FinancialUIMap::deleteExpenseBtn);
    expenseFooter->addStretch(1);
    m_expenseTotalCaption = roleLabel(QString(), "desc");
    expenseFooter->addWidget(m_expenseTotalCaption);
    m_expenseTotal = roleLabel(QString(), "cardTitle");
    expenseFooter->addWidget(m_expenseTotal);
    expenseColumn->addLayout(expenseFooter);

    auto* lower = new QHBoxLayout;
    lower->setSpacing(10);
    lower->addLayout(leftColumn, 3);
    lower->addWidget(expenseCard, 2);
    overview->addLayout(lower, 1);

    // ── Yearly Summary ──────────────────────────────────────────────────────
    clearPageLayout(ui->Tab2);

    auto* annual = new QVBoxLayout(ui->Tab2);
    annual->setContentsMargins(10, 10, 10, 10);
    annual->setSpacing(10);

    auto* annualBar = new QHBoxLayout;
    annualBar->setSpacing(6);
    m_yearCaption = roleLabel(QString(), "sectionTitle");
    annualBar->addWidget(m_yearCaption, 1);
    FinancialUIMap::yearlySelect->setMinimumWidth(120);
    annualBar->addWidget(FinancialUIMap::yearlySelect);
    annual->addLayout(annualBar);

    QVBoxLayout* chartColumn = nullptr;
    QFrame* chartCard = makeCard(chartColumn, 4);
    auto* chartHeader = new QHBoxLayout;
    m_chartTitle = roleLabel(QString(), "cardTitle");
    chartHeader->addWidget(m_chartTitle, 1);
    m_yearTotal = roleLabel(QString(), "cardTitle");
    chartHeader->addWidget(m_yearTotal);
    chartColumn->addLayout(chartHeader);
    m_yearChart = new YearChart;
    m_yearChart->setFixedHeight(YearChartHeight);
    m_yearChart->setCursor(Qt::PointingHandCursor);
    chartColumn->addWidget(m_yearChart);
    annual->addWidget(chartCard);

    QVBoxLayout* yearColumn = nullptr;
    QFrame* yearCard = makeCard(yearColumn, 0);
    QTableWidget* yearly = FinancialUIMap::yearlyTable;
    usePlainStyle(yearly);
    for (int column = 0; column < yearly->columnCount(); ++column) {
        yearly->horizontalHeaderItem(column)->setTextAlignment(
            (column == FinancialConstants::COL_YEAR_MONTH ? Qt::AlignLeft : Qt::AlignRight) | Qt::AlignVCenter);
    }
    yearColumn->addWidget(yearly, 1);
    annual->addWidget(yearCard, 1);
}
