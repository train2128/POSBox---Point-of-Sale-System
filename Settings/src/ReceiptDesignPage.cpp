#include "ReceiptDesignPage.h"
#include "LineIcons.h"
#include "LocalSettings.h"
#include "Prefixes.h"
#include "LocalApi.h"
#include "ThermalPrinter.h"
#include <QButtonGroup>
#include <QComboBox>
#include <QCoreApplication>
#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <QtMath>
#include <algorithm>

namespace {

constexpr int PaperMargin = 16;     // white edge around the printing, in dots
constexpr int LineHeight = 30;
constexpr int SmallLineHeight = 22;
constexpr int CharWidth = 12;
constexpr int SmallCharWidth = 9;
constexpr int LogoGap = 12;
constexpr int ZigzagHeight = 8;
constexpr int ShownPaperWidth = 340;  // on screen, for 58 mm paper

const QColor PaperColor("#FCFCFA");
const QColor InkColor("#111111");

QFont receiptFont(bool small, bool bold)
{
    QFont font;
    font.setFamilies({"DejaVu Sans Mono", "Consolas", "Liberation Mono", "Courier New"});
    font.setStyleHint(QFont::Monospace);
    font.setPixelSize(small ? 14 : 19);
    font.setBold(bold);
    return font;
}

int lineHeight(const Receipt::Line& line)
{
    return (line.small ? SmallLineHeight : LineHeight) * (line.doubleHeight ? 2 : 1);
}

// The height of the paper in dots, with one rectangle per clickable part.
template <typename HitAdder>
int measure(const QList<Receipt::Line>& lines, const QImage& logo, int dots, HitAdder addHit)
{
    int y = 20;
    for (const Receipt::Line& line : lines) {
        if (line.isLogo) {
            const QImage bitmap = Receipt::logoBitmap(logo, line.logoDots);
            addHit(QRectF(PaperMargin, y, dots, bitmap.height()), line.part);
            y += bitmap.height() + LogoGap;
            continue;
        }
        const int height = lineHeight(line);
        if (!line.part.isEmpty())
            addHit(QRectF(PaperMargin, y, dots, height), line.part);
        const int charWidth = (line.small ? SmallCharWidth : CharWidth) * (line.doubleWidth ? 2 : 1);
        const int textWidth = int(line.text.size()) * charWidth;
        const int textX = PaperMargin + (line.align == 1 ? (dots - textWidth) / 2 : line.align == 2 ? dots - textWidth : 0);
        for (const Receipt::Span& span : line.spans)
            addHit(QRectF(textX + span.start * charWidth, y, span.length * charWidth, height), span.part);
        y += height;
    }
    return y + 26;
}

void paintPaper(QPainter& painter, const QList<Receipt::Line>& lines, const QImage& logo, int dots, int height)
{
    const int width = dots + 2 * PaperMargin;
    QPainterPath shape;
    shape.moveTo(0, 0);
    shape.lineTo(width, 0);
    shape.lineTo(width, height);
    const int tooth = 12;
    for (int x = width; x > 0; x -= tooth) {
        shape.lineTo(x - tooth / 2.0, height + ZigzagHeight);
        shape.lineTo(x - tooth, height);
    }
    shape.closeSubpath();
    painter.setPen(QPen(QColor(0, 0, 0, 50), 1));
    painter.setBrush(PaperColor);
    painter.drawPath(shape);

    int y = 20;
    for (const Receipt::Line& line : lines) {
        if (line.isLogo) {
            const QImage bitmap = Receipt::logoBitmap(logo, line.logoDots);
            const int x = PaperMargin + (line.align == 1 ? (dots - bitmap.width()) / 2 : 0);
            painter.drawImage(x, y, bitmap);
            y += bitmap.height() + LogoGap;
            continue;
        }
        const int cellWidth = line.small ? SmallCharWidth : CharWidth;
        const int cellHeight = line.small ? SmallLineHeight : LineHeight;
        const int scaleX = line.doubleWidth ? 2 : 1;
        const int scaleY = line.doubleHeight ? 2 : 1;
        const int textWidth = int(line.text.size()) * cellWidth * scaleX;
        const int x = PaperMargin + (line.align == 1 ? (dots - textWidth) / 2 : line.align == 2 ? dots - textWidth : 0);
        if (line.reversed)
            painter.fillRect(QRectF(PaperMargin, y, dots, cellHeight * scaleY), InkColor);
        painter.setFont(receiptFont(line.small, line.bold));
        painter.setPen(line.reversed ? PaperColor : InkColor);
        painter.save();
        painter.translate(x, y);
        painter.scale(scaleX, scaleY);
        for (int i = 0; i < line.text.size(); ++i)
            if (line.text.at(i) != QChar(' '))
                painter.drawText(QRectF(i * cellWidth, 0, cellWidth, cellHeight), Qt::AlignCenter, QString(line.text.at(i)));
        painter.restore();
        y += cellHeight * scaleY;
    }
}

QLabel* roleLabel(const QString& text, const char* role)
{
    auto* label = new QLabel(text);
    label->setProperty("uiRole", role);
    return label;
}

QFrame* designCard(QVBoxLayout** body, const QString& title)
{
    auto* frame = new QFrame;
    frame->setProperty("uiCard", true);
    auto* layout = new QVBoxLayout(frame);
    layout->setContentsMargins(14, 12, 14, 14);
    layout->setSpacing(10);
    layout->addWidget(roleLabel(title, "cardTitle"));
    *body = layout;
    return frame;
}

QScrollArea* plainScroll(QWidget* content)
{
    auto* scroll = new QScrollArea;
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidget(content);
    scroll->setStyleSheet("QScrollArea, QScrollArea > QWidget > QWidget { background: transparent; }");
    return scroll;
}

// Lets a combo share a narrow column instead of pushing it wider.
void fitToColumn(QComboBox* combo)
{
    combo->setObjectName("receiptCombo");
    combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    combo->setMinimumContentsLength(4);
}

QWidget* labelled(const QString& title, QWidget* control)
{
    auto* holder = new QWidget;
    auto* layout = new QVBoxLayout(holder);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    layout->addWidget(roleLabel(title, "field"));
    layout->addWidget(control);
    return holder;
}

} // namespace

// ── The receipt ─────────────────────────────────────────────────────────────

ReceiptPreview::ReceiptPreview(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setAttribute(Qt::WA_Hover);
}

void ReceiptPreview::setTicket(const Receipt::TicketData& data, const QString& paperSize)
{
    m_data = data;
    m_paperSize = paperSize;
    m_hits.clear();
    m_paperHeight = measure(Receipt::layout(m_data, m_paperSize), m_data.shop.logoImage,
                            Receipt::dotsFor(m_paperSize),
                            [this](const QRectF& rect, const QString& part) { m_hits << Hit{rect, part}; });
    updateHeight();
    setHovered(partAt(mapFromGlobal(QCursor::pos())));
    update();
}

QImage ReceiptPreview::renderPaper(const Receipt::TicketData& data, const QString& paperSize)
{
    const int dots = Receipt::dotsFor(paperSize);
    const QList<Receipt::Line> lines = Receipt::layout(data, paperSize);
    const int height = measure(lines, data.shop.logoImage, dots, [](const QRectF&, const QString&) {});
    QImage image(dots + 2 * PaperMargin, height + ZigzagHeight + 1, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    paintPaper(painter, lines, data.shop.logoImage, dots, height);
    return image;
}

qreal ReceiptPreview::paperScale() const
{
    const int dots = Receipt::dotsFor(m_paperSize);
    const int paperWidth = dots + 2 * PaperMargin;
    const qreal wanted = ShownPaperWidth / qreal(Receipt::dotsFor("58mm") + 2 * PaperMargin);
    // The circle sticks out a little past the paper's edge.
    return qMin(wanted, (width() - 18) / qreal(paperWidth));
}

QRectF ReceiptPreview::paperRect() const
{
    const qreal scale = paperScale();
    const qreal paperWidth = (Receipt::dotsFor(m_paperSize) + 2 * PaperMargin) * scale;
    return QRectF(0, 0, paperWidth, (m_paperHeight + ZigzagHeight) * scale);
}

QString ReceiptPreview::partAt(const QPointF& position) const
{
    const qreal scale = paperScale();
    if (scale <= 0)
        return {};
    const QPointF dot = position / scale;
    for (const Hit& hit : m_hits)
        if (hit.rect.contains(dot))
            return hit.part;
    return {};
}

void ReceiptPreview::setHovered(const QString& part)
{
    if (part == m_hovered)
        return;
    m_hovered = part;
    setCursor(part.isEmpty() ? Qt::ArrowCursor : Qt::PointingHandCursor);
    update();
}

void ReceiptPreview::updateHeight()
{
    const int height = qCeil(paperRect().height()) + 2;
    if (minimumHeight() != height)
        setMinimumHeight(height);
}

void ReceiptPreview::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateHeight();
}

void ReceiptPreview::mouseMoveEvent(QMouseEvent* event)
{
    setHovered(partAt(event->position()));
}

void ReceiptPreview::leaveEvent(QEvent* event)
{
    setHovered(QString());
    QWidget::leaveEvent(event);
}

void ReceiptPreview::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton)
        return;
    const QString part = partAt(event->position());
    if (!part.isEmpty())
        emit partClicked(part);
}

void ReceiptPreview::paintEvent(QPaintEvent*)
{
    if (m_paperSize.isEmpty())
        return;
    QPainter painter(this);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
    const qreal scale = paperScale();
    const int dots = Receipt::dotsFor(m_paperSize);

    painter.save();
    painter.scale(scale, scale);
    paintPaper(painter, Receipt::layout(m_data, m_paperSize), m_data.shop.logoImage, dots, m_paperHeight);
    painter.restore();

    if (m_hovered.isEmpty())
        return;

    // Outline the part: lines next to each other draw as one block.
    const QColor accent = LineIcons::colors().accent;
    QList<QRectF> blocks;
    for (const Hit& hit : m_hits) {
        if (hit.part != m_hovered)
            continue;
        const QRectF rect(hit.rect.x() * scale, hit.rect.y() * scale,
                          hit.rect.width() * scale, hit.rect.height() * scale);
        if (!blocks.isEmpty() && qAbs(blocks.last().bottom() - rect.top()) < 1 && qAbs(blocks.last().left() - rect.left()) < 1)
            blocks.last().setBottom(rect.bottom());
        else
            blocks << rect;
    }
    QColor fill = accent;
    fill.setAlphaF(0.12);
    for (const QRectF& block : blocks) {
        painter.setPen(QPen(accent, 1.6, Qt::DashLine));
        painter.setBrush(fill);
        painter.drawRoundedRect(block.adjusted(-2, -1, 2, 1), 5, 5);
    }
    if (blocks.isEmpty())
        return;

    // A circle at the paper's edge, with what a click does beside it when there's room.
    const QRectF first = blocks.first();
    const qreal radius = 13;
    const QPointF center(paperRect().right(), first.center().y());
    painter.setPen(Qt::NoPen);
    painter.setBrush(accent);
    painter.drawEllipse(center, radius, radius);
    painter.setPen(QPen(Qt::white, 2, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(center + QPointF(-4, -4), center + QPointF(4, 4));
    painter.drawLine(center + QPointF(4, -4), center + QPointF(-4, 4));

    const QString words = Receipt::partName(m_hovered) + ":\nclick to hide";
    QFont font = this->font();
    font.setBold(true);
    painter.setFont(font);
    const QRectF room(center.x() + radius + 6, center.y() - 24, width() - center.x() - radius - 8, 48);
    if (room.width() >= 90) {
        painter.setPen(accent);
        painter.drawText(room, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap, words);
    } else {
        const QString oneLine = Receipt::partName(m_hovered);
        const QFontMetrics metrics(font);
        const QRectF pill(center.x() - radius - 6 - metrics.horizontalAdvance(oneLine) - 16,
                          center.y() - metrics.height() / 2.0 - 3, metrics.horizontalAdvance(oneLine) + 16, metrics.height() + 6);
        painter.setPen(Qt::NoPen);
        painter.setBrush(accent);
        painter.drawRoundedRect(pill, pill.height() / 2, pill.height() / 2);
        painter.setPen(Qt::white);
        painter.drawText(pill, Qt::AlignCenter, oneLine);
    }
}

// ── The page ────────────────────────────────────────────────────────────────

ReceiptDesignPage::ReceiptDesignPage(LocalApi* api, LocalImages* images, QWidget* parent)
    : QWidget(parent)
    , m_api(api)
    , m_images(images)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(30, 22, 30, 22);
    layout->setSpacing(16);

    auto* words = new QVBoxLayout;
    words->setSpacing(0);
    words->addWidget(roleLabel(tr("Receipt"), "pageTitle"));
    words->addWidget(roleLabel(tr("What prints on your tickets"), "pageSub"));
    layout->addLayout(words);

    auto* columns = new QHBoxLayout;
    columns->setSpacing(12);
    columns->addWidget(buildModelRail());
    columns->addWidget(buildPreview(), 1);
    columns->addWidget(buildControls());
    layout->addLayout(columns, 1);
}

QWidget* ReceiptDesignPage::buildModelRail()
{
    QVBoxLayout* body;
    QFrame* frame = designCard(&body, tr("Models"));
    frame->setFixedWidth(166);
    body->setContentsMargins(10, 12, 10, 14);

    auto* list = new QWidget;
    auto* listLayout = new QVBoxLayout(list);
    listLayout->setContentsMargins(0, 0, 0, 0);
    listLayout->setSpacing(6);
    m_models = new QButtonGroup(this);
    for (int model = 1; model <= Receipt::ModelCount; ++model) {
        auto* tile = new QPushButton(Receipt::modelName(model));
        tile->setToolTip(Receipt::modelName(model));   // the name may be cut short in some languages
        tile->setObjectName("receiptModelTile");
        tile->setCheckable(true);
        tile->setCursor(Qt::PointingHandCursor);
        tile->setIconSize(QSize(40, 50));
        tile->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        m_models->addButton(tile, model);
        m_modelButtons << tile;
        listLayout->addWidget(tile);
    }
    listLayout->addStretch();
    connect(m_models, &QButtonGroup::idClicked, this, &ReceiptDesignPage::chooseModel);
    body->addWidget(plainScroll(list), 1);
    return frame;
}

QWidget* ReceiptDesignPage::buildPreview()
{
    QVBoxLayout* body;
    QFrame* frame = designCard(&body, tr("Click any part to hide or show it"));
    frame->setMinimumWidth(236);
    frame->findChild<QLabel*>()->setWordWrap(true);

    m_preview = new ReceiptPreview;
    connect(m_preview, &ReceiptPreview::partClicked, this, &ReceiptDesignPage::togglePart);

    auto* holder = new QWidget;
    auto* holderLayout = new QVBoxLayout(holder);
    holderLayout->setContentsMargins(0, 0, 0, 0);
    holderLayout->addWidget(m_preview);
    holderLayout->addStretch();
    // Always there: the paper's height follows its width, so a scrollbar that
    // came and went would resize the paper, and that would toggle it again.
    QScrollArea* scroll = plainScroll(holder);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    body->addWidget(scroll, 1);
    return frame;
}

QWidget* ReceiptDesignPage::buildControls()
{
    QVBoxLayout* body;
    QFrame* frame = designCard(&body, tr("Settings"));
    frame->setFixedWidth(276);

    auto* form = new QWidget;
    auto* formLayout = new QVBoxLayout(form);
    formLayout->setContentsMargins(0, 0, 4, 0);
    formLayout->setSpacing(10);

    m_paper = new QComboBox;
    m_paper->addItems({"58mm", "76mm", "80mm", "82.5mm", "104mm", "112mm"});
    fitToColumn(m_paper);
    connect(m_paper, &QComboBox::currentTextChanged, this, [this](const QString& size) {
        emit paperSizeChosen(size);
        refresh();
    });
    formLayout->addWidget(labelled(tr("Paper"), m_paper));

    m_top = new QLineEdit;
    m_top->setMaxLength(120);
    connect(m_top, &QLineEdit::textEdited, this, [this](const QString& value) {
        m_design.topMessage = value.trimmed();
        edited();
    });
    formLayout->addWidget(labelled(tr("Top message"), m_top));

    m_bottom = new QLineEdit;
    m_bottom->setMaxLength(120);
    connect(m_bottom, &QLineEdit::textEdited, this, [this](const QString& value) {
        m_design.bottomMessage = value.trimmed();
        edited();
    });
    formLayout->addWidget(labelled(tr("Bottom message"), m_bottom));

    // The stored value travels as item data; the shown text is translated.
    const auto choice = [this](const QList<QPair<QString, QString>>& values,
                               QString Receipt::Design::*member) {
        auto* combo = new QComboBox;
        for (const auto& value : values)
            combo->addItem(value.second, value.first);
        fitToColumn(combo);
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [this, member, combo](int) {
                    m_design.*member = combo->currentData().toString();
                    edited();
                });
        return combo;
    };
    m_logoSize = choice({{"Small", tr("Small")}, {"Medium", tr("Medium")}, {"Large", tr("Large")}},
                        &Receipt::Design::logoSize);
    m_totalSize = choice({{"Normal", tr("Normal")}, {"Large", tr("Large")}, {"Huge", tr("Huge")},
                          {"Band", tr("Band")}}, &Receipt::Design::totalSize);
    m_lines = choice({{"Dashes", tr("Dashes")}, {"Double", tr("Double")}, {"Dots", tr("Dots")},
                      {"None", tr("None")}}, &Receipt::Design::lines);
    m_spacing = choice({{"Smallest", tr("Smallest")}, {"Compact", tr("Compact")}, {"Normal", tr("Normal")},
                        {"Airy", tr("Airy")}}, &Receipt::Design::spacing);

    auto* styles = new QGridLayout;
    styles->setHorizontalSpacing(10);
    styles->setVerticalSpacing(10);
    styles->setColumnStretch(0, 1);
    styles->setColumnStretch(1, 1);
    styles->addWidget(labelled(tr("Logo size"), m_logoSize), 0, 0);
    styles->addWidget(labelled(tr("Total size"), m_totalSize), 0, 1);
    styles->addWidget(labelled(tr("Lines"), m_lines), 1, 0);
    styles->addWidget(labelled(tr("Spacing"), m_spacing), 1, 1);
    formLayout->addLayout(styles);

    m_chipsTitle = roleLabel(tr("HIDDEN PARTS"), "cap");
    formLayout->addWidget(m_chipsTitle);
    m_chips = new QWidget;
    m_chips->installEventFilter(this);
    formLayout->addWidget(m_chips);
    formLayout->addStretch();
    body->addWidget(plainScroll(form), 1);

    m_testButton = new QPushButton(tr("Test Print"));
    m_testButton->setCursor(Qt::PointingHandCursor);
    connect(m_testButton, &QPushButton::clicked, this, &ReceiptDesignPage::testPrint);
    body->addWidget(m_testButton);

    m_saveButton = new QPushButton(tr("Save"));
    m_saveButton->setProperty("btnStyle", "add");
    m_saveButton->setCursor(Qt::PointingHandCursor);
    connect(m_saveButton, &QPushButton::clicked, this, &ReceiptDesignPage::save);
    body->addWidget(m_saveButton);
    return frame;
}

QString ReceiptDesignPage::paperSize() const
{
    return m_paper->currentText();
}

void ReceiptDesignPage::setPaperSize(const QString& paperSize)
{
    const QSignalBlocker blocker(m_paper);
    m_paper->setCurrentText(paperSize);
    if (isVisible())
        refresh();
}

void ReceiptDesignPage::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    load();
}

bool ReceiptDesignPage::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_chips && event->type() == QEvent::Resize)
        placeChips();
    return QWidget::eventFilter(watched, event);
}

void ReceiptDesignPage::load()
{
    m_sample = Receipt::sampleTicket(m_api, m_images);
    if (m_sample.shop.logoImage.isNull()) {
        // Where a logo would go, until the shop has one.
        QImage placeholder(240, 240, QImage::Format_RGB32);
        placeholder.fill(Qt::white);
        QPainter painter(&placeholder);
        painter.setPen(QPen(Qt::black, 6, Qt::DashLine));
        painter.drawRect(placeholder.rect().adjusted(6, 6, -6, -6));
        QFont font = painter.font();
        font.setPixelSize(56);
        font.setBold(true);
        painter.setFont(font);
        painter.drawText(placeholder.rect(), Qt::AlignCenter, "LOGO");
        painter.end();
        m_sample.shop.logoImage = placeholder;
    }
    // Changes not saved yet survive a trip to another page.
    if (!m_edited)
        m_design = m_sample.design;
    refreshControls();
    refreshThumbnails();
    refresh();
}

void ReceiptDesignPage::refresh()
{
    Receipt::TicketData data = m_sample;
    data.design = m_design;
    m_preview->setTicket(data, paperSize());
    refreshChips();
}

void ReceiptDesignPage::refreshThumbnails()
{
    for (int model = 1; model <= Receipt::ModelCount; ++model) {
        Receipt::TicketData data = m_sample;
        data.design = Receipt::Design();
        Receipt::applyModel(data.design, model);
        const QImage paper = ReceiptPreview::renderPaper(data, "58mm");
        const QImage scaled = paper.scaledToWidth(80, Qt::SmoothTransformation);
        QImage cropped(80, 100, QImage::Format_ARGB32_Premultiplied);
        cropped.fill(PaperColor);
        QPainter painter(&cropped);
        painter.drawImage(0, 0, scaled);
        painter.end();
        QPixmap thumbnail = QPixmap::fromImage(cropped);
        thumbnail.setDevicePixelRatio(2.0);
        m_modelButtons[model - 1]->setIcon(thumbnail);
    }
}

void ReceiptDesignPage::refreshControls()
{
    const QSignalBlocker b(m_logoSize), c(m_totalSize), d(m_lines), e(m_spacing);
    m_modelButtons[qBound(1, m_design.model, Receipt::ModelCount) - 1]->setChecked(true);
    const auto select = [](QComboBox* combo, const QString& value) {
        const int index = combo->findData(value);
        if (index >= 0) combo->setCurrentIndex(index);
    };
    select(m_logoSize, m_design.logoSize);
    select(m_totalSize, m_design.totalSize);
    select(m_lines, m_design.lines);
    select(m_spacing, m_design.spacing);
    m_top->setText(m_design.topMessage);
    m_bottom->setText(m_design.bottomMessage);
    // The receipt's own wording, in the app language.
    m_top->setPlaceholderText(m_design.model == 7 ? QCoreApplication::translate("Receipt", "Welcome!")
                                                  : tr("None"));
    m_bottom->setPlaceholderText(QCoreApplication::translate("Receipt", "Thank you, see you soon!"));
}

void ReceiptDesignPage::refreshChips()
{
    qDeleteAll(m_chipButtons);
    m_chipButtons.clear();

    QStringList parts(m_design.hidden.cbegin(), m_design.hidden.cend());
    std::sort(parts.begin(), parts.end(), [](const QString& a, const QString& b) {
        return Receipt::partName(a) < Receipt::partName(b);
    });
    const QIcon plus = LineIcons::icon("plus", LineIcons::colors().accent, 14);
    for (const QString& part : parts) {
        auto* chip = new QPushButton(Receipt::partName(part), m_chips);
        chip->setObjectName("receiptChip");
        chip->setCursor(Qt::PointingHandCursor);
        chip->setIcon(plus);
        chip->setIconSize(QSize(14, 14));
        chip->setToolTip(tr("Click to show it again"));
        connect(chip, &QPushButton::clicked, this, [this, part]() {
            // Runs from the chip's own click, and the refresh replaces every chip.
            QMetaObject::invokeMethod(this, [this, part]() { togglePart(part); }, Qt::QueuedConnection);
        });
        chip->show();
        m_chipButtons << chip;
    }
    m_chipsTitle->setVisible(!parts.isEmpty());
    placeChips();
}

void ReceiptDesignPage::placeChips()
{
    const int room = m_chips->width();
    int x = 0, y = 0, rowHeight = 0;
    for (QPushButton* chip : std::as_const(m_chipButtons)) {
        chip->ensurePolished();
        const QSize size = chip->sizeHint();
        if (x > 0 && x + size.width() > room) {
            x = 0;
            y += rowHeight + 6;
            rowHeight = 0;
        }
        chip->setGeometry(x, y, qMin(size.width(), room), size.height());
        x += size.width() + 6;
        rowHeight = qMax(rowHeight, size.height());
    }
    m_chips->setFixedHeight(y + rowHeight);
}

void ReceiptDesignPage::edited()
{
    m_edited = true;
    refresh();
}

void ReceiptDesignPage::chooseModel(int model)
{
    Receipt::applyModel(m_design, model);
    refreshControls();
    edited();
}

void ReceiptDesignPage::togglePart(const QString& part)
{
    if (m_design.hidden.contains(part))
        m_design.hidden.remove(part);
    else
        m_design.hidden.insert(part);
    edited();
}

void ReceiptDesignPage::testPrint()
{
    Receipt::TicketData data = m_sample;
    data.design = m_design;
    QVariantMap settings = LocalSettings::load().printer;
    settings["paper_size"] = paperSize();
    ThermalPrinter printer(settings);
    if (printer.print(data))
        AppHelpers::showInfo(this, tr("The test receipt was sent to the printer."));
    else
        AppHelpers::showError(this, tr("Could not print the test receipt.\n\n") + printer.lastError());
}

void ReceiptDesignPage::save()
{
    if (!m_api || !m_api->update("shop_profile", {{"receipt_design", Receipt::toJson(m_design)}}, {{"id", 1}})) {
        AppHelpers::showError(this, tr("Could not save the receipt design.\n\n") + (m_api ? m_api->lastError() : QString()));
        return;
    }
    LocalSettings local = LocalSettings::load();
    local.printer["paper_size"] = paperSize();
    local.save();
    m_edited = false;
    AppHelpers::showInfo(this, tr("Your receipts will now print this way."));
}

void ReceiptDesignPage::updateIcons()
{
    const LineIcons::Colors colors = LineIcons::colors();
    m_testButton->setIcon(LineIcons::icon("printer", colors.text, 16));
    m_saveButton->setIcon(LineIcons::icon("check", Qt::white, 16));
    if (!m_chipButtons.isEmpty()) {
        const QIcon plus = LineIcons::icon("plus", colors.accent, 14);
        for (QPushButton* chip : std::as_const(m_chipButtons))
            chip->setIcon(plus);
    }
    m_preview->update();
}
