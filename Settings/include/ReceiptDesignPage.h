#ifndef RECEIPTDESIGNPAGE_H
#define RECEIPTDESIGNPAGE_H

#include "ReceiptDesign.h"
#include <QHash>
#include <QPointer>
#include <QWidget>

class QButtonGroup;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class LocalApi;
class LocalImages;

// The receipt on paper, drawn on the printer's dot grid. Parts that can be
// hidden highlight under the mouse and report a click.
class ReceiptPreview : public QWidget
{
    Q_OBJECT
public:
    explicit ReceiptPreview(QWidget* parent = nullptr);

    void setTicket(const Receipt::TicketData& data, const QString& paperSize);

    // The same drawing at full size, for the model thumbnails.
    static QImage renderPaper(const Receipt::TicketData& data, const QString& paperSize);

signals:
    void partClicked(const QString& part);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    struct Hit {
        QRectF rect;    // in paper dots
        QString part;
    };
    qreal paperScale() const;
    QRectF paperRect() const;
    QString partAt(const QPointF& position) const;
    void setHovered(const QString& part);
    void updateHeight();

    Receipt::TicketData m_data;
    QString m_paperSize;
    QList<Hit> m_hits;
    int m_paperHeight = 0;
    QString m_hovered;
};

// Settings page where the receipt is designed by clicking on it.
class ReceiptDesignPage : public QWidget
{
    Q_OBJECT
public:
    ReceiptDesignPage(LocalApi* api, LocalImages* images, QWidget* parent = nullptr);

    QPushButton* testButton() const { return m_testButton; }
    QPushButton* saveButton() const { return m_saveButton; }
    QString paperSize() const;
    void setPaperSize(const QString& paperSize);

    void updateIcons();

signals:
    void paperSizeChosen(const QString& paperSize);

protected:
    void showEvent(QShowEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QWidget* buildModelRail();
    QWidget* buildPreview();
    QWidget* buildControls();
    void load();
    void refresh();
    void refreshThumbnails();
    void refreshControls();
    void refreshChips();
    void placeChips();
    void edited();
    void chooseModel(int model);
    void togglePart(const QString& part);
    void testPrint();
    void save();

    LocalApi* m_api;
    LocalImages* m_images;
    Receipt::TicketData m_sample;
    Receipt::Design m_design;
    bool m_edited = false;

    ReceiptPreview* m_preview = nullptr;
    QButtonGroup* m_models = nullptr;
    QList<QPushButton*> m_modelButtons;
    QComboBox* m_paper = nullptr;
    QLineEdit* m_top = nullptr;
    QLineEdit* m_bottom = nullptr;
    QComboBox* m_logoSize = nullptr;
    QComboBox* m_totalSize = nullptr;
    QComboBox* m_lines = nullptr;
    QComboBox* m_spacing = nullptr;
    QLabel* m_chipsTitle = nullptr;
    QWidget* m_chips = nullptr;
    QList<QPushButton*> m_chipButtons;
    QPushButton* m_testButton = nullptr;
    QPushButton* m_saveButton = nullptr;
};

#endif // RECEIPTDESIGNPAGE_H
