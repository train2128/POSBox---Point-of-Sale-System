#ifndef SAVETICKETDIALOG_H
#define SAVETICKETDIALOG_H

#include <QDialog>
#include <QString>

class QLineEdit;
class QDoubleSpinBox;
class QTextEdit;

/**
 * Dialog shown when saving the current cart as an OPEN ticket.
 * Also shown when re-saving an open ticket during edit mode,
 * pre-filled with existing values.
 */
class SaveTicketDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SaveTicketDialog(QWidget* parent = nullptr);

    // Pre-fill fields when editing an existing open ticket
    void setCustomerName(const QString& name);
    void setAdvancePayment(double amount);
    void setDescription(const QString& desc);

    // Getters — call after exec() == Accepted
    QString customerName()    const;
    double  advancePayment()  const;
    QString description()     const;

private:
    void setupUI();

    QLineEdit*      m_nameField;
    QDoubleSpinBox* m_advanceSpin;
    QTextEdit*      m_descField;
};

#endif // SAVETICKETDIALOG_H
