#ifndef OPENTICKETSDIALOG_H
#define OPENTICKETSDIALOG_H

#include "ModernDialog.h"

class LocalApi;
class QTableWidget;
class QPushButton;

/**
 * Dialog showing all OPEN tickets.
 * Allows the cashier to Complete, Edit, or Delete an open ticket.
 */
class OpenTicketsDialog : public ModernDialog
{
    Q_OBJECT

public:
    explicit OpenTicketsDialog(LocalApi* api, QWidget* parent = nullptr);

    // Result — check after exec() == Accepted
    enum class Action { None, Complete, Edit, Print };

    Action selectedAction() const { return m_action; }
    int    selectedTicketId() const { return m_selectedTicketId; }

private slots:
    void onSelectionChanged();
    void onCompleteClicked();
    void onEditClicked();
    void onDeleteClicked();
    void onPrintClicked();

private:
    void setupUI();
    void loadTickets();
    void setButtonsEnabled(bool enabled);
    int  currentTicketId() const;

    LocalApi* m_api;
    QTableWidget*    m_table;
    QPushButton*     m_completeBtn;
    QPushButton*     m_editBtn;
    QPushButton*     m_deleteBtn;
    QPushButton*     m_printBtn;

    Action m_action;
    int    m_selectedTicketId;
};

#endif // OPENTICKETSDIALOG_H
