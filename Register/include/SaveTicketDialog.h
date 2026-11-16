#ifndef SAVETICKETDIALOG_H
#define SAVETICKETDIALOG_H

#include "ModernDialog.h"
#include <QList>
#include <QString>

class QLabel;
class QLineEdit;
class QTextEdit;
class QVBoxLayout;

/**
 * Dialog shown when saving the current cart as an OPEN ticket.
 * Also shown when re-saving an open ticket during edit mode,
 * pre-filled with existing values. The side panel shows what is left to pay.
 */
class SaveTicketDialog : public ModernDialog
{
    Q_OBJECT

public:
    struct Line {
        QString text;
        double amount;
    };

    explicit SaveTicketDialog(QWidget* parent = nullptr);

    void setSummary(const QList<Line>& lines, double discount, double total);

    // Pre-fill fields when editing an existing open ticket
    void setCustomerName(const QString& name);
    void setAdvancePayment(double amount);
    void setDescription(const QString& desc);

    // Getters: call after exec() == Accepted
    QString customerName()    const;
    double  advancePayment()  const;
    QString description()     const;

private:
    void updateLeftToPay();

    QLineEdit* m_nameField;
    QLineEdit* m_advanceEdit;
    QTextEdit* m_descField;
    QVBoxLayout* m_lines;
    QLabel* m_totalValue;
    QLabel* m_advanceValue;
    QLabel* m_leftValue;
    double m_total = 0.0;
};

#endif // SAVETICKETDIALOG_H
