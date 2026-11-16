#ifndef MODERNDIALOG_H
#define MODERNDIALOG_H

#include "CoreExport.h"
#include <QDialog>
#include <QString>

class QFrame;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QPainter;
class QPixmap;
class QRect;
class QPushButton;
class QVBoxLayout;

// The frame shared by POSBox's small windows: rounded card with a soft shadow,
// icon, title and subtitle, close button, and a footer with the main action.
//   Card: labels above the fields, quiet Cancel beside the main button.
//   Band: colored top band, fields in one grouped card, full-width main button.
// The window has no system title bar, so it is dragged by its header. Colors and
// font sizes live in the stylesheets.
class CORE_EXPORT ModernDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Look { Card, Band };
    enum class Tone { Accent, Green, Red, Amber };

    ModernDialog(Look look, const QString& icon, Tone tone, const QString& title,
                 const QString& subtitle, int width, QWidget* parent = nullptr);

    QVBoxLayout* body() const { return m_body; }
    void setSubtitle(const QString& text);
    // Shows a picture in the header in place of the line icon.
    void setHeaderPicture(const QPixmap& picture);

    // Card look: a label above the field; two fields sharing one row.
    void addField(const QString& label, QWidget* field);
    void addFieldPair(const QString& label1, QWidget* field1, const QString& label2, QWidget* field2);
    // Band look: one row of the grouped card, label on the left.
    void addRow(const QString& label, QWidget* editor);

    // A panel on the right of the window. Returns its layout.
    QVBoxLayout* addSidePanel(const QString& title, int width = 290);

    // Adds Cancel and the main button (not connected yet) and returns the latter.
    QPushButton* addFooter(const QString& text, const QString& icon, const char* buttonStyle);
    // Band look: only the text-style Cancel.
    void addCancelLink(const QString& text = QString());   // empty: "Cancel" in the app language
    // Card look: the footer's divider and an empty row to fill by hand.
    QHBoxLayout* addCustomFooter();

    // A quiet outlined button with an optional icon, in the theme's text color.
    QPushButton* quietButton(const QString& text, const QString& icon = QString());
    // A main button in one of the stylesheet's button styles, with a white icon.
    QPushButton* mainButton(const QString& text, const QString& icon, const char* buttonStyle);
    // A soft colored line: label on the left, value on the right.
    QFrame* resultStrip(QLabel** label, QLabel** value);

    static QColor toneColor(Tone tone);
    // The soft shadow around a rounded card. `window` is the whole window, which
    // keeps ShadowMargin free on every side.
    static void paintCardShadow(QPainter& painter, const QRect& window);
    static constexpr int ShadowMargin = 14;
    // An amount field: up to 3 decimals, dot or comma, currency shown inside.
    static QLineEdit* moneyEdit(QWidget* parent, bool big = false);
    static double moneyValue(const QLineEdit* edit);

protected:
    void paintEvent(QPaintEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QWidget* buildHeader(const QString& icon, Tone tone, const QString& title, const QString& subtitle);

    Look m_look;
    QHBoxLayout* m_columns = nullptr;
    QVBoxLayout* m_main = nullptr;
    QVBoxLayout* m_body = nullptr;
    QVBoxLayout* m_groupLayout = nullptr;
    QLabel* m_subtitle = nullptr;
    QLabel* m_headerIcon = nullptr;
    QWidget* m_header = nullptr;
    bool m_hasSide = false;
};

#endif // MODERNDIALOG_H
