#ifndef MESSAGEDIALOG_H
#define MESSAGEDIALOG_H

#include "CoreExport.h"
#include <QDialog>
#include <QString>

class QLabel;
class QPushButton;
class QVBoxLayout;

// The app's message window: a centered icon, title and text in a rounded card.
class CORE_EXPORT MessageDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Kind { Success, Warning, Error, Question, Danger };

    MessageDialog(Kind kind, const QString& title, const QString& text, QWidget* parent = nullptr);

    // A large green line under the text, such as the amount of a sale.
    void setAmount(const QString& amount);
    // The main button and, if not empty, a quiet one beside it. By default a
    // single OK. exec() returns Accepted for the main button only.
    void setButtons(const QString& mainText, const QString& quietText = QString(),
                    const QString& mainIcon = QString());

protected:
    void paintEvent(QPaintEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    Kind m_kind;
    QWidget* m_card = nullptr;
    QVBoxLayout* m_column = nullptr;
    QWidget* m_buttons = nullptr;
    QLabel* m_amount = nullptr;
};

#endif // MESSAGEDIALOG_H
