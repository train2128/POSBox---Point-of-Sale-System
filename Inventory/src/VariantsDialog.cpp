#include "VariantsDialog.h"
#include "InventoryConstants.h"
#include "LocalApi.h"
#include "Prefixes.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QFrame>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QIntValidator>
#include <QSignalBlocker>
#include <QStyle>
#include <QTimer>
#include <QEvent>
#include <QDebug>

// ===== HELPERS =====

namespace {
constexpr int DialogWidth = 620;
constexpr int MaxListHeight = 440;
}

static QString stripCurrency(const QString& text)
{
    QString result = text;
    result.remove(QString(" ") + AppConstants::CURRENCY, Qt::CaseInsensitive);
    return result.replace(',', '.').trimmed();
}

static void connectNumberFormat(QLineEdit* edit)
{
    QObject::connect(edit, &QLineEdit::editingFinished, edit, [edit]() {
        const QString raw = stripCurrency(edit->text());
        if (raw.isEmpty()) return;
        bool ok = false;
        const double val = raw.toDouble(&ok);
        if (ok) edit->setText(QString::number(val, 'f', AppConstants::PRICE_DECIMALS));
    });
}

static QString amountText(double val)
{
    return val > 0 ? QString::number(val, 'f', AppConstants::PRICE_DECIMALS) : QString();
}

// Row labels show the parsed value of the raw text, blank when unparseable.
static QString priceDisplay(const QString& raw)
{
    bool ok = false;
    const double v = stripCurrency(raw).toDouble(&ok);
    return ok && v > 0 ? AppHelpers::formatPrice(v) : QString();
}

static void repolish(QWidget* w)
{
    w->style()->unpolish(w);
    w->style()->polish(w);
}

// ===== CONSTRUCTOR =====

VariantsDialog::VariantsDialog(LocalApi* api,
                               const QList<PendingVariant>& pending,
                               int productId,
                               bool isService,
                               const QString& productName,
                               QWidget* parent)
    : ModernDialog(Look::Band, "grid", Tone::Accent, tr("Manage variants"), QString(), DialogWidth, parent)
    , m_api(api)
    , m_productId(productId)
    , m_isService(isService)
    , m_productName(productName)
{
    setWindowTitle(tr("Manage Variants"));
    setupUI();
    setupConnections();

    if (!pending.isEmpty()) {
        populateFromPending(pending);
    } else if (m_productId > 0) {
        loadExistingVariants();
    }
    // No data yet: one empty row, ready to fill.
    if (m_rows.isEmpty()) {
        VariantRow row;
        row.stockText = QStringLiteral("0");
        appendRow(row);
    }

    selectRow(0);
}

// ===== SETUP =====

void VariantsDialog::setupUI()
{
    m_listWidget = new QWidget;
    m_listWidget->setObjectName("variantRows");
    m_listLayout = new QVBoxLayout(m_listWidget);
    m_listLayout->setContentsMargins(0, 0, 0, 0);
    m_listLayout->setSpacing(8);
    m_listLayout->addStretch(1);

    m_scroll = new QScrollArea(this);
    m_scroll->setObjectName("variantScroll");
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->setWidgetResizable(true);
    m_scroll->setWidget(m_listWidget);
    body()->addWidget(m_scroll);

    m_editor = buildEditor();

    QPushButton* addBtn = quietButton(tr("Add variant"), "plus");
    connect(addBtn, &QPushButton::clicked, this, &VariantsDialog::onAddVariant);
    body()->addWidget(addBtn);

    QPushButton* save = addFooter(tr("Save Variants"), "check", "add");
    connect(save, &QPushButton::clicked, this, &VariantsDialog::onAccepted);
}

// The fields of the open row. One set, moved to whichever row is open.
QWidget* VariantsDialog::buildEditor()
{
    auto* editor = new QWidget;
    editor->setObjectName("variantEditor");
    auto* layout = new QVBoxLayout(editor);
    layout->setContentsMargins(14, 0, 14, 12);
    layout->setSpacing(10);

    const auto column = [editor](const QString& text, QWidget* field) {
        auto* box = new QWidget(editor);
        auto* boxLayout = new QVBoxLayout(box);
        boxLayout->setContentsMargins(0, 0, 0, 0);
        boxLayout->setSpacing(5);
        auto* caption = new QLabel(text, box);
        caption->setObjectName("modalFieldLabel");
        boxLayout->addWidget(caption);
        boxLayout->addWidget(field);
        return box;
    };

    m_nameEdit = new QLineEdit(editor);
    m_nameEdit->setPlaceholderText(tr("Variant name"));
    m_costEdit = moneyEdit(editor);
    m_priceEdit = moneyEdit(editor);
    m_stockEdit = new QLineEdit(editor);
    m_stockEdit->setPlaceholderText("0");
    m_stockEdit->setValidator(new QIntValidator(0, 999999, this));
    connectNumberFormat(m_costEdit);
    connectNumberFormat(m_priceEdit);

    auto* fields = new QHBoxLayout;
    fields->setSpacing(8);
    fields->addWidget(column(tr("Name"), m_nameEdit), 1);
    fields->addWidget(column(tr("Cost"), m_costEdit), 1);
    fields->addWidget(column(tr("Price"), m_priceEdit), 1);
    // A service has no stock: the field is hidden entirely (not disabled).
    m_stockColumn = column(tr("Stock"), m_stockEdit);
    m_stockColumn->setVisible(!m_isService);
    fields->addWidget(m_stockColumn, 1);
    layout->addLayout(fields);

    auto* bottom = new QHBoxLayout;
    QLabel* marginCaption = nullptr;
    QFrame* strip = resultStrip(&marginCaption, &m_marginValue);
    marginCaption->setText(tr("Margin"));
    m_deleteBtn = quietButton(tr("Delete variant"), "trash");
    bottom->addWidget(strip, 1);
    bottom->addWidget(m_deleteBtn);
    layout->addLayout(bottom);
    return editor;
}

void VariantsDialog::setupConnections()
{
    connect(m_deleteBtn, &QPushButton::clicked,
            this, &VariantsDialog::onDeleteVariant);

    // Detail edits flow straight into the selected row (raw text is the
    // source of truth; parsing happens on save).
    connect(m_nameEdit,  &QLineEdit::textChanged,
            this, &VariantsDialog::onDetailEdited);
    connect(m_costEdit,  &QLineEdit::textChanged,
            this, &VariantsDialog::onDetailEdited);
    connect(m_priceEdit, &QLineEdit::textChanged,
            this, &VariantsDialog::onDetailEdited);
    connect(m_stockEdit, &QLineEdit::textChanged,
            this, &VariantsDialog::onDetailEdited);
}

// ===== ROW MANAGEMENT =====

void VariantsDialog::loadExistingVariants()
{
    if (!m_api || m_productId <= 0) return;

    const QList<DataRow> rows = m_api->variants(m_productId);
    if (!m_api->ok()) {
        qWarning() << "Failed to load variants:" << m_api->lastError();
        return;
    }

    for (const DataRow& v : rows) {
        VariantRow row;
        row.variantId = v.value("id").toInt();
        row.nameText  = v.value("variant_name").toString();
        row.costText  = amountText(v.value("cost").toDouble());
        row.priceText = amountText(v.value("price").toDouble());
        row.stockText = QString::number(v.value("stock").toInt());
        appendRow(row);
    }
}

void VariantsDialog::populateFromPending(const QList<PendingVariant>& pending)
{
    for (const PendingVariant& pv : pending) {
        VariantRow row;
        row.variantId = pv.variantId;
        row.nameText  = pv.name;
        row.costText  = amountText(pv.cost);
        row.priceText = amountText(pv.price);
        row.stockText = QString::number(pv.stock);
        appendRow(row);
    }
}

void VariantsDialog::appendRow(const VariantRow& row)
{
    VariantRow r = row;

    r.container = new QFrame(m_listWidget);
    r.container->setObjectName("variantRow");
    r.container->show();
    auto* rowLayout = new QVBoxLayout(r.container);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(0);

    r.header = new QWidget(r.container);
    r.header->setCursor(Qt::PointingHandCursor);
    r.header->setFixedHeight(50);
    auto* headerLayout = new QHBoxLayout(r.header);
    headerLayout->setContentsMargins(14, 0, 14, 0);
    headerLayout->setSpacing(10);

    r.nameLabel = new QLabel(r.header);
    r.nameLabel->setObjectName("modalChoiceTitle");
    r.priceLabel = new QLabel(r.header);
    r.priceLabel->setObjectName("discountLineValue");
    r.stockLabel = new QLabel(r.header);
    r.stockLabel->setObjectName("variantStock");
    r.stockLabel->setVisible(!m_isService);

    // Rows are click-to-select; the labels must not eat the press.
    for (QLabel* label : {r.nameLabel, r.priceLabel, r.stockLabel})
        label->setAttribute(Qt::WA_TransparentForMouseEvents);

    headerLayout->addWidget(r.nameLabel, 1);
    headerLayout->addWidget(r.priceLabel);
    headerLayout->addWidget(r.stockLabel);
    rowLayout->addWidget(r.header);

    r.header->installEventFilter(this);

    m_listLayout->insertWidget(m_rows.size(), r.container);
    updateRowLabels(r);

    m_rows.append(r);
    updateSubtitle();
}

void VariantsDialog::updateRowLabels(VariantRow& row)
{
    row.nameLabel->setText(row.nameText.trimmed().isEmpty() ? tr("New variant") : row.nameText);
    row.nameLabel->setEnabled(!row.nameText.trimmed().isEmpty());
    row.priceLabel->setText(priceDisplay(row.priceText));
    const int stock = row.stockText.toInt();
    row.stockLabel->setText(tr("%1 left").arg(stock));
    row.stockLabel->setProperty("level", stock <= 0 ? "out" : stock <= 3 ? "low" : "ok");
    repolish(row.stockLabel);
}

void VariantsDialog::removeRowAt(int index)
{
    if (index < 0 || index >= m_rows.size()) return;

    // The shared editor must not go down with its row.
    m_editor->setParent(nullptr);
    m_editor->hide();

    delete m_rows[index].container; // also removes it from the layout
    m_rows.removeAt(index);
    m_selected = -1;

    updateSubtitle();

    if (m_rows.isEmpty()) {
        clearDetail();
        updateListHeight();
        return;
    }
    selectRow(qMin(index, int(m_rows.size()) - 1));
}

void VariantsDialog::selectRow(int index)
{
    if (index < 0 || index >= m_rows.size()) return;

    if (m_selected >= 0 && m_selected < m_rows.size()) {
        m_rows[m_selected].container->setProperty("rowState", QVariant());
        repolish(m_rows[m_selected].container);
    }

    m_selected = index;
    VariantRow& row = m_rows[index];
    row.container->setProperty("rowState", "selected");
    repolish(row.container);

    static_cast<QVBoxLayout*>(row.container->layout())->addWidget(m_editor);
    m_editor->show();

    m_nameEdit->setEnabled(true);
    m_costEdit->setEnabled(true);
    m_priceEdit->setEnabled(true);
    m_stockEdit->setEnabled(true);
    m_deleteBtn->setEnabled(true);

    // Block signals: setText() would fire textChanged → onDetailEdited, which
    // copies the not-yet-updated sibling fields back over the row's raw text.
    {
        const QSignalBlocker b1(m_nameEdit), b2(m_costEdit),
                             b3(m_priceEdit), b4(m_stockEdit);
        m_nameEdit->setText(row.nameText);
        m_costEdit->setText(row.costText);
        m_priceEdit->setText(row.priceText);
        m_stockEdit->setText(row.stockText);
    }

    refreshDetailChip();
    updateListHeight();
    m_scroll->ensureWidgetVisible(row.container, 0, 8);
    m_nameEdit->setFocus(Qt::OtherFocusReason);
    // Qt delivers the deferred pre-show focus with TabFocusReason, which makes
    // the line edit select-all; a plain cursor at the end is nicer.
    QTimer::singleShot(0, m_nameEdit, [this] { m_nameEdit->deselect(); });
}

// The list takes the room it needs, then scrolls.
void VariantsDialog::updateListHeight()
{
    constexpr int HeaderAndBorder = 52;
    int height = int(m_rows.size()) * HeaderAndBorder
                 + qMax(0, int(m_rows.size()) - 1) * m_listLayout->spacing();
    if (m_selected >= 0)
        height += m_editor->sizeHint().height();
    m_scroll->setFixedHeight(qMin(height, MaxListHeight));
}

// The editor's size is only known once everything is on screen.
void VariantsDialog::showEvent(QShowEvent* event)
{
    ModernDialog::showEvent(event);
    updateListHeight();
}

void VariantsDialog::clearDetail()
{
    m_selected = -1;
    m_nameEdit->clear();
    m_costEdit->clear();
    m_priceEdit->clear();
    m_stockEdit->clear();
    m_editor->hide();
}

void VariantsDialog::updateSubtitle()
{
    const QString base = m_productName.isEmpty() ? tr("New product")
                                                 : m_productName;
    const int n = m_rows.size();
    // Whole sentences, so every language can word its own plural.
    setSubtitle(n == 1 ? tr("%1  ·  1 variant").arg(base)
                       : tr("%1  ·  %2 variants").arg(base).arg(n));
}

// ===== SLOTS =====

void VariantsDialog::onAddVariant()
{
    VariantRow row;
    row.stockText = QStringLiteral("0");
    appendRow(row);
    selectRow(m_rows.size() - 1);
}

void VariantsDialog::onDeleteVariant()
{
    if (m_selected < 0 || m_selected >= m_rows.size()) return;
    removeRowAt(m_selected);
}

void VariantsDialog::onDetailEdited()
{
    if (m_selected < 0 || m_selected >= m_rows.size()) return;

    VariantRow& row = m_rows[m_selected];
    row.nameText  = m_nameEdit->text();
    row.costText  = m_costEdit->text();
    row.priceText = m_priceEdit->text();
    row.stockText = m_stockEdit->text();

    updateRowLabels(row);
    refreshDetailChip();
}

// Live margin, same format as the product window.
void VariantsDialog::refreshDetailChip()
{
    bool costOk = false, priceOk = false;
    const double cost  = stripCurrency(m_costEdit->text()).toDouble(&costOk);
    const double price = stripCurrency(m_priceEdit->text()).toDouble(&priceOk);

    if (!costOk || !priceOk || price <= 0.0) {
        m_marginValue->setText("-");
        m_marginValue->setProperty("loss", false);
    } else {
        const double delta = price - cost;
        m_marginValue->setProperty("loss", delta < 0.0);
        m_marginValue->setText(QString("%1%  ·  %2%3")
                                   .arg(delta / price * 100.0, 0, 'f', 1)
                                   .arg(delta >= 0 ? QStringLiteral("+") : QString())
                                   .arg(delta, 0, 'f', AppConstants::PRICE_DECIMALS));
    }
    repolish(m_marginValue);
}

void VariantsDialog::onAccepted()
{
    if (validateAndCollect()) {
        accept();
    }
}

bool VariantsDialog::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        for (int i = 0; i < m_rows.size(); ++i) {
            if (m_rows[i].header == obj) {
                selectRow(i);
                return true;
            }
        }
    }
    return ModernDialog::eventFilter(obj, event);
}

// ===== ACCEPT =====

bool VariantsDialog::validateAndCollect()
{
    if (m_rows.isEmpty()) {
        AppHelpers::showWarning(this, AppHelpers::trMsg(InventoryConstants::MSG_VARIANT_NO_VARIANTS));
        return false;
    }

    QList<PendingVariant> result;

    for (int i = 0; i < m_rows.size(); ++i) {
        const VariantRow& row = m_rows[i];

        const QString name = row.nameText.trimmed();
        if (name.isEmpty()) {
            AppHelpers::showWarning(this, tr("Variant %1: %2")
                                            .arg(i + 1)
                                            .arg(AppHelpers::trMsg(InventoryConstants::MSG_VARIANT_NAME_EMPTY)));
            return false;
        }

        bool costOk = false, priceOk = false;
        const double cost  = stripCurrency(row.costText).toDouble(&costOk);
        const double price = stripCurrency(row.priceText).toDouble(&priceOk);

        if (!costOk) {
            AppHelpers::showWarning(this, tr("Variant %1: %2")
                                            .arg(i + 1)
                                            .arg(AppHelpers::trMsg(InventoryConstants::MSG_VARIANT_COST_INVALID)));
            return false;
        }
        if (!priceOk) {
            AppHelpers::showWarning(this, tr("Variant %1: %2")
                                            .arg(i + 1)
                                            .arg(AppHelpers::trMsg(InventoryConstants::MSG_VARIANT_PRICE_INVALID)));
            return false;
        }

        const int stock = m_isService ? 0 : row.stockText.toInt();
        result.append(PendingVariant(row.variantId, name, cost, price, stock));
    }

    m_pendingVariants = result;
    qDebug() << "VariantsDialog: collected" << m_pendingVariants.size() << "variants";
    return true;
}
