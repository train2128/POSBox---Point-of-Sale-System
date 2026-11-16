#include "OpenTicketsDialog.h"
#include "LocalApi.h"
#include "LineIcons.h"
#include "Prefixes.h"
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QDebug>

// Column indices
static const int COL_DATE    = 0;
static const int COL_NAME    = 1;
static const int COL_DESC    = 2;
static const int COL_TOTAL   = 3;
static const int COL_ADVANCE = 4;
static const int COL_LEFT    = 5;
static const int COL_COUNT   = 6;

OpenTicketsDialog::OpenTicketsDialog(LocalApi* api, QWidget* parent)
    : ModernDialog(Look::Card, "receipt", Tone::Accent, tr("Open tickets"), QString(), 780, parent)
    , m_api(api)
    , m_table(nullptr)
    , m_completeBtn(nullptr)
    , m_editBtn(nullptr)
    , m_deleteBtn(nullptr)
    , m_printBtn(nullptr)
    , m_action(Action::None)
    , m_selectedTicketId(-1)
{
    setupUI();
    loadTickets();
}

void OpenTicketsDialog::setupUI()
{
    m_table = new QTableWidget(0, COL_COUNT, this);
    m_table->setObjectName("openTicketsTable");
    m_table->setHorizontalHeaderLabels({ tr("DATE"), tr("CUSTOMER"), tr("DESCRIPTION"), tr("TOTAL"), tr("ADVANCE"), tr("LEFT")});
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setFocusPolicy(Qt::NoFocus);
    m_table->setShowGrid(false);
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setDefaultSectionSize(44);
    QHeaderView* header = m_table->horizontalHeader();
    header->setHighlightSections(false);
    header->setSectionResizeMode(QHeaderView::ResizeToContents);
    header->setSectionResizeMode(COL_DESC, QHeaderView::Stretch);
    header->setMinimumSectionSize(84);
    for (int column : {COL_TOTAL, COL_ADVANCE, COL_LEFT})
        m_table->model()->setHeaderData(column, Qt::Horizontal,
                                        int(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
    for (int column : {COL_DATE, COL_NAME, COL_DESC})
        m_table->model()->setHeaderData(column, Qt::Horizontal,
                                        int(Qt::AlignLeft | Qt::AlignVCenter), Qt::TextAlignmentRole);
    m_table->setFixedHeight(280);
    body()->addWidget(m_table);

    QHBoxLayout* btnRow = addCustomFooter();

    QPushButton* closeBtn = quietButton(tr("Close"));
    m_deleteBtn = mainButton(tr("Delete"), "trash", "outlineDanger");
    m_deleteBtn->setIcon(LineIcons::icon("trash", toneColor(Tone::Red), 16));
    m_deleteBtn->setAutoDefault(false);
    m_printBtn = quietButton(tr("Print"), "printer");
    m_editBtn = quietButton(tr("Edit"), "edit");
    m_completeBtn = mainButton(tr("Complete"), "check", "add");
    m_completeBtn->setAutoDefault(false);

    btnRow->addWidget(closeBtn);
    btnRow->addStretch();
    btnRow->addWidget(m_deleteBtn);
    btnRow->addWidget(m_printBtn);
    btnRow->addWidget(m_editBtn);
    btnRow->addWidget(m_completeBtn);

    setButtonsEnabled(false);

    connect(m_table,       &QTableWidget::itemSelectionChanged,
            this, &OpenTicketsDialog::onSelectionChanged);
    connect(m_completeBtn, &QPushButton::clicked,
            this, &OpenTicketsDialog::onCompleteClicked);
    connect(m_editBtn,     &QPushButton::clicked,
            this, &OpenTicketsDialog::onEditClicked);
    connect(m_deleteBtn,   &QPushButton::clicked,
            this, &OpenTicketsDialog::onDeleteClicked);
    connect(m_printBtn,    &QPushButton::clicked,
            this, &OpenTicketsDialog::onPrintClicked);
    connect(closeBtn,      &QPushButton::clicked,
            this, &QDialog::reject);
}

void OpenTicketsDialog::loadTickets()
{
    if (!m_api) return;

    m_table->clearSpans();
    m_table->setRowCount(0);

    // Open tickets, ordered by due date.
    const QList<DataRow> rows = m_api->openTickets();
    if (!m_api->ok()) {
        qWarning() << "OpenTicketsDialog: failed to load open tickets:"
                   << m_api->lastError();
        return;
    }

    double allTotals = 0.0;
    double allLeft = 0.0;
    for (const DataRow& r : rows) {
        const int     ticketId = r.value("id").toInt();
        const QString date     = r.value("date").toString();
        const QString custName = r.value("customer_name").toString();
        const double  total    = r.value("total").toDouble();
        const double  advance  = r.value("deposit_amount").toDouble();
        const QString desc     = r.value("description").toString();
        const double  left     = qMax(0.0, total - advance);
        allTotals += total;
        allLeft += left;

        const int row = m_table->rowCount();
        m_table->insertRow(row);

        auto makeItem = [&](const QString& text, bool number = false) {
            QTableWidgetItem* item = new QTableWidgetItem(text);
            item->setTextAlignment((number ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter);
            item->setData(Qt::UserRole, ticketId);
            return item;
        };

        m_table->setItem(row, COL_DATE,    makeItem(AppHelpers::formatDate(
                                            QDate::fromString(date, "yyyy-MM-dd"))));
        auto* nameItem = makeItem(custName.isEmpty() ? "-" : custName);
        QFont nameFont = m_table->font();
        nameFont.setBold(true);
        nameItem->setFont(nameFont);
        m_table->setItem(row, COL_NAME,    nameItem);
        m_table->setItem(row, COL_DESC,    makeItem(desc.isEmpty() ? "-" : desc));
        m_table->setItem(row, COL_TOTAL,   makeItem(AppHelpers::formatAmount(total), true));
        m_table->setItem(row, COL_ADVANCE, makeItem(AppHelpers::formatAmount(advance), true));
        auto* leftItem = makeItem(AppHelpers::formatAmount(left), true);
        leftItem->setForeground(toneColor(Tone::Amber));
        leftItem->setFont(nameFont);
        m_table->setItem(row, COL_LEFT,    leftItem);
    }

    setSubtitle(rows.isEmpty()
                    ? tr("Nothing saved")
                    : tr("%1 saved  ·  %2 total, %3 still to pay")
                          .arg(rows.size())
                          .arg(AppHelpers::formatPrice(allTotals), AppHelpers::formatAmount(allLeft)));

    if (m_table->rowCount() == 0) {
        m_table->insertRow(0);
        QTableWidgetItem* placeholder = new QTableWidgetItem(tr("No open tickets"));
        placeholder->setTextAlignment(Qt::AlignCenter);
        placeholder->setFlags(Qt::NoItemFlags);
        m_table->setItem(0, 0, placeholder);
        m_table->setSpan(0, 0, 1, COL_COUNT);
    }
}

void OpenTicketsDialog::setButtonsEnabled(bool enabled)
{
    m_completeBtn->setEnabled(enabled);
    m_editBtn->setEnabled(enabled);
    m_deleteBtn->setEnabled(enabled);
    m_printBtn->setEnabled(enabled);
}

int OpenTicketsDialog::currentTicketId() const
{
    const QList<QTableWidgetItem*> selected = m_table->selectedItems();
    if (selected.isEmpty()) return -1;
    return selected.first()->data(Qt::UserRole).toInt();
}

void OpenTicketsDialog::onSelectionChanged()
{
    setButtonsEnabled(currentTicketId() > 0);
}

void OpenTicketsDialog::onCompleteClicked()
{
    const int id = currentTicketId();
    if (id <= 0) return;

    if (!AppHelpers::confirmAction(this,
                                   tr("Mark this ticket as completed and deduct stock?"))) return;

    m_selectedTicketId = id;
    m_action = Action::Complete;
    accept();
}

void OpenTicketsDialog::onEditClicked()
{
    const int id = currentTicketId();
    if (id <= 0) return;

    m_selectedTicketId = id;
    m_action = Action::Edit;
    accept();
}

void OpenTicketsDialog::onDeleteClicked()
{
    const int id = currentTicketId();
    if (id <= 0) return;

    if (!AppHelpers::confirmAction(this,
                                   tr("Delete this open ticket? This cannot be undone."))) return;

    if (!m_api->deleteTicket(id)) {
        AppHelpers::showError(this, tr("Failed to delete ticket.\n\n") + m_api->lastError());
        return;
    }

    loadTickets();
    setButtonsEnabled(false);
}

void OpenTicketsDialog::onPrintClicked()
{
    const int id = currentTicketId();
    if (id <= 0) return;

    m_selectedTicketId = id;
    m_action = Action::Print;
    accept();
}
