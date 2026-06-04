#include "OpenTicketsDialog.h"
#include "DatabaseManager.h"
#include "QueryRepository.h"
#include "Prefixes.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QPushButton>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

// Column indices
static const int COL_DATE    = 0;
static const int COL_NAME    = 1;
static const int COL_TOTAL   = 2;
static const int COL_ADVANCE = 3;
static const int COL_DESC    = 4;
static const int COL_COUNT   = 5;

OpenTicketsDialog::OpenTicketsDialog(DatabaseManager* db, QWidget* parent)
    : QDialog(parent)
    , m_db(db)
    , m_table(nullptr)
    , m_completeBtn(nullptr)
    , m_editBtn(nullptr)
    , m_deleteBtn(nullptr)
    , m_action(Action::None)
    , m_selectedTicketId(-1)
{
    setWindowTitle("Open Tickets");
    setModal(true);
    setMinimumSize(700, 400);
    setupUI();
    loadTickets();
}

void OpenTicketsDialog::setupUI()
{
    QVBoxLayout* main = new QVBoxLayout(this);
    main->setContentsMargins(16, 16, 16, 16);
    main->setSpacing(12);

    QLabel* title = new QLabel("Open Tickets", this);
    title->setAlignment(Qt::AlignCenter);
    main->addWidget(title);

    m_table = new QTableWidget(0, COL_COUNT, this);
    m_table->setHorizontalHeaderLabels({
        "Date", "Customer", "Total", "Advance", "Description"
    });
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(COL_DESC, QHeaderView::Stretch);
    m_table->setShowGrid(false);
    main->addWidget(m_table);

    QHBoxLayout* btnRow = new QHBoxLayout();
    btnRow->setSpacing(8);

    m_completeBtn = new QPushButton("Complete", this);
    m_completeBtn->setProperty("btnStyle", "add");
    m_completeBtn->style()->unpolish(m_completeBtn);
    m_completeBtn->style()->polish(m_completeBtn);

    m_editBtn = new QPushButton("Edit", this);
    m_editBtn->setProperty("btnStyle", "edit");
    m_editBtn->style()->unpolish(m_editBtn);
    m_editBtn->style()->polish(m_editBtn);

    m_deleteBtn = new QPushButton("Delete", this);
    m_deleteBtn->setProperty("btnStyle", "delete");
    m_deleteBtn->style()->unpolish(m_deleteBtn);
    m_deleteBtn->style()->polish(m_deleteBtn);

    m_printBtn = new QPushButton("Print", this);
    m_printBtn->setProperty("btnStyle", "add");
    m_printBtn->style()->unpolish(m_printBtn);
    m_printBtn->style()->polish(m_printBtn);

    QPushButton* closeBtn = new QPushButton("Close", this);

    btnRow->addWidget(m_completeBtn);
    btnRow->addWidget(m_editBtn);
    btnRow->addWidget(m_deleteBtn);
    btnRow->addWidget(m_printBtn);
    btnRow->addStretch();
    btnRow->addWidget(closeBtn);

    main->addLayout(btnRow);

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
    if (!m_db) return;

    m_table->setRowCount(0);

    // Local query includes description (TICKET_SELECT_OPEN omits it)
    QSqlQuery query(m_db->database());
    query.prepare(
        "SELECT id, date, customer_name, total, deposit_amount, description "
        "FROM tickets WHERE status = 'OPEN' ORDER BY date DESC");

    if (!query.exec()) {
        qWarning() << "OpenTicketsDialog: failed to load open tickets:"
                   << query.lastError().text();
        return;
    }

    while (query.next()) {
        const int     ticketId = query.value("id").toInt();
        const QString date     = query.value("date").toString();
        const QString custName = query.value("customer_name").toString();
        const double  total    = query.value("total").toDouble();
        const double  advance  = query.value("deposit_amount").toDouble();
        const QString desc     = query.value("description").toString();

        const int row = m_table->rowCount();
        m_table->insertRow(row);

        auto makeItem = [&](const QString& text) {
            QTableWidgetItem* item = new QTableWidgetItem(text);
            item->setTextAlignment(Qt::AlignCenter);
            item->setData(Qt::UserRole, ticketId);
            return item;
        };

        m_table->setItem(row, COL_DATE,    makeItem(AppHelpers::formatDate(
                                            QDate::fromString(date, "yyyy-MM-dd"))));
        m_table->setItem(row, COL_NAME,    makeItem(custName.isEmpty() ? "-" : custName));
        m_table->setItem(row, COL_TOTAL,   makeItem(AppHelpers::formatPrice(total)));
        m_table->setItem(row, COL_ADVANCE, makeItem(AppHelpers::formatPrice(advance)));
        m_table->setItem(row, COL_DESC,    makeItem(desc.isEmpty() ? "-" : desc));
    }

    if (m_table->rowCount() == 0) {
        m_table->insertRow(0);
        QTableWidgetItem* placeholder = new QTableWidgetItem("No open tickets");
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
                                   "Mark this ticket as completed and deduct stock?")) return;

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
                                   "Delete this open ticket? This cannot be undone.")) return;

    QSqlQuery del(m_db->database());
    del.prepare(Queries::TICKET_ITEM_DELETE_BY_TICKET);
    del.addBindValue(id);
    del.exec();

    QSqlQuery del2(m_db->database());
    del2.prepare(Queries::TICKET_DELETE);
    del2.addBindValue(id);

    if (!del2.exec()) {
        AppHelpers::showError(this, "Failed to delete ticket.\n\n" + del2.lastError().text());
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
