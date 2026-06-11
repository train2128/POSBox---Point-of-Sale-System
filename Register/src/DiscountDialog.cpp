#include "DiscountDialog.h"
#include "DatabaseManager.h"
#include "QueryRepository.h"
#include "Prefixes.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QFrame>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

DiscountDialog::DiscountDialog(DatabaseManager* db, double subtotal, QWidget* parent)
    : QDialog(parent)
    , m_db(db)
    , m_subtotal(subtotal)
    , m_presetCombo(nullptr)
    , m_customEdit(nullptr)
    , m_previewLabel(nullptr)
{
    setObjectName("discount-dialog");
    setWindowTitle(tr("Apply Discount"));
    setModal(true);
    setFixedWidth(340);

    setupUI();
    loadPresets();
    updatePreview();
}

DiscountDialog::~DiscountDialog() {}

void DiscountDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(14);

    // Title
    QLabel* titleLabel = new QLabel(tr("Apply Discount"), this);
    titleLabel->setObjectName("discount-dialog-title");
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);

    // ── Preset section ────────────────────────────────────────────
    QLabel* presetLabel = new QLabel(tr("Preset Discount"), this);
    presetLabel->setObjectName("discount-dialog-section-label");
    mainLayout->addWidget(presetLabel);

    m_presetCombo = new QComboBox(this);
    m_presetCombo->setObjectName("discount-dialog-combo");
    mainLayout->addWidget(m_presetCombo);

    // ── Divider ───────────────────────────────────────────────────
    QFrame* divider = new QFrame(this);
    divider->setFrameShape(QFrame::HLine);
    divider->setObjectName("discount-dialog-divider");
    mainLayout->addWidget(divider);

    // ── Custom section ────────────────────────────────────────────
    QLabel* customLabel = new QLabel(tr("Custom Discount"), this);
    customLabel->setObjectName("discount-dialog-section-label");
    mainLayout->addWidget(customLabel);

    QLabel* hintLabel = new QLabel(tr("Enter amount (e.g. 5 or 0.5) or percentage (e.g. 5%)"), this);
    hintLabel->setObjectName("discount-dialog-hint");
    hintLabel->setWordWrap(true);
    mainLayout->addWidget(hintLabel);

    m_customEdit = new QLineEdit(this);
    m_customEdit->setObjectName("discount-dialog-input");
    m_customEdit->setPlaceholderText(tr("e.g. 5   or   10%"));
    mainLayout->addWidget(m_customEdit);

    // ── Preview ───────────────────────────────────────────────────
    m_previewLabel = new QLabel(this);
    m_previewLabel->setObjectName("discount-dialog-preview");
    m_previewLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_previewLabel);

    // ── Buttons ───────────────────────────────────────────────────
    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(8);

    QPushButton* okBtn = new QPushButton(tr("Apply"), this);
    okBtn->setObjectName("discount-dialog-ok");
    okBtn->setProperty("btnStyle", "add");
    okBtn->style()->unpolish(okBtn);
    okBtn->style()->polish(okBtn);

    QPushButton* cancelBtn = new QPushButton(tr("Cancel"), this);
    cancelBtn->setObjectName("discount-dialog-cancel");
    cancelBtn->setProperty("btnStyle", "delete");
    cancelBtn->style()->unpolish(cancelBtn);
    cancelBtn->style()->polish(cancelBtn);

    btnLayout->addWidget(okBtn);
    btnLayout->addWidget(cancelBtn);
    mainLayout->addLayout(btnLayout);

    connect(okBtn,         &QPushButton::clicked,        this, &DiscountDialog::onOkClicked);
    connect(cancelBtn,     &QPushButton::clicked,        this, &QDialog::reject);
    connect(m_customEdit,  &QLineEdit::textChanged,      this, &DiscountDialog::onCustomInputChanged);
    connect(m_presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DiscountDialog::onPresetChanged);
}

void DiscountDialog::loadPresets()
{
    m_presetCombo->addItem(tr("--- None ---"), -1);

    if (!m_db) return;

    QSqlQuery query(m_db->database());
    query.prepare(Queries::DISCOUNT_SELECT_ACTIVE);

    if (!query.exec()) {
        qWarning() << "DiscountDialog: failed to load presets:" << query.lastError().text();
        return;
    }

    while (query.next()) {
        int     id    = query.value("id").toInt();
        QString name  = query.value("name").toString();
        QString type  = query.value("type").toString();
        double  value = query.value("value").toDouble();

        QString label;
        if (type == "percentage")
            label = QString("%1  (%2%)").arg(name).arg(value, 0, 'f', 1);
        else
            label = QString("%1  (%2)").arg(name).arg(AppHelpers::formatPrice(value));

        // Store id, type, value packed into a QVariantList
        QVariantList data = { id, type, value };
        m_presetCombo->addItem(label, QVariant::fromValue(data));
    }
}

void DiscountDialog::onCustomInputChanged(const QString&)
{
    // When user types in custom field, clear preset selection
    if (!m_customEdit->text().trimmed().isEmpty())
        m_presetCombo->setCurrentIndex(0);  // "— None —"

    updatePreview();
}

void DiscountDialog::onPresetChanged(int)
{
    // When user picks a preset, clear custom field
    QVariant data = m_presetCombo->currentData();
    if (data.isValid() && data.toList().value(0).toInt() != -1)
        m_customEdit->clear();

    updatePreview();
}

void DiscountDialog::updatePreview()
{
    DiscountItem preview;
    bool valid = false;

    QString customText = m_customEdit->text().trimmed();

    if (!customText.isEmpty()) {
        // Custom input takes priority
        bool isPercent = customText.endsWith('%');
        QString numStr = isPercent ? customText.chopped(1).trimmed() : customText;

        bool ok = false;
        double val = numStr.toDouble(&ok);

        if (ok && val > 0) {
            preview.type  = isPercent ? "percentage" : "amount";
            preview.value = val;
            preview.name  = isPercent ? QString("%1%").arg(val) : AppHelpers::formatPrice(val);
            valid = true;
        }
    } else {
        // Check preset
        QVariant data = m_presetCombo->currentData();
        if (data.isValid()) {
            QVariantList list = data.toList();
            int id = list.value(0).toInt();
            if (id != -1) {
                preview.presetId = id;
                preview.type     = list.value(1).toString();
                preview.value    = list.value(2).toDouble();
                preview.name     = m_presetCombo->currentText().split("  (").first().trimmed();
                valid = true;
            }
        }
    }

    if (valid) {
        double amount = DiscountHandler::calculateAmount(preview, m_subtotal);
        m_previewLabel->setText(
            QString(tr("Discount: -%1")).arg(AppHelpers::formatPrice(amount)));
    } else {
        m_previewLabel->setText(tr("No discount selected"));
    }
}

void DiscountDialog::onOkClicked()
{
    QString customText = m_customEdit->text().trimmed();

    if (!customText.isEmpty()) {
        bool isPercent = customText.endsWith('%');
        QString numStr = isPercent ? customText.chopped(1).trimmed() : customText;

        bool ok = false;
        double val = numStr.toDouble(&ok);

        if (!ok || val <= 0) {
            AppHelpers::showWarning(this, tr("Please enter a valid discount value."));
            m_customEdit->setFocus();
            return;
        }

        m_result.type     = isPercent ? "percentage" : "amount";
        m_result.value    = val;
        m_result.name     = isPercent
                            ? QString(tr("Custom (%1%)")).arg(val)
                            : QString(tr("Custom (%1)")).arg(AppHelpers::formatPrice(val));
        m_result.presetId = -1;
        accept();
        return;
    }

    // No custom input — check preset
    QVariant data = m_presetCombo->currentData();
    if (data.isValid()) {
        QVariantList list = data.toList();
        int id = list.value(0).toInt();

        if (id == -1) {
            AppHelpers::showWarning(this, tr("Please select a preset or enter a custom discount."));
            return;
        }

        m_result.presetId = id;
        m_result.type     = list.value(1).toString();
        m_result.value    = list.value(2).toDouble();
        m_result.name     = m_presetCombo->currentText().split("  (").first().trimmed();
        accept();
        return;
    }

    AppHelpers::showWarning(this, tr("Please select a preset or enter a custom discount."));
}

DiscountItem DiscountDialog::result() const
{
    return m_result;
}
