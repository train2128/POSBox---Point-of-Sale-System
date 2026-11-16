#include "DiscountDialog.h"
#include "LocalApi.h"
#include "Prefixes.h"
#include <QButtonGroup>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QDebug>

namespace {
constexpr int FormWidth = 420;
constexpr int ChipAreaWidth = FormWidth - 44;
constexpr int ChipGap = 8;
constexpr int MaxChipAreaHeight = 124;
}

DiscountDialog::DiscountDialog(LocalApi* api, double subtotal, QWidget* parent)
    : ModernDialog(Look::Card, "tag", Tone::Amber, tr("Apply discount"),
                   tr("Pick a preset, or type an amount or a percentage"), FormWidth, parent)
    , m_api(api)
    , m_subtotal(subtotal)
    , m_presetGroup(new QButtonGroup(this))
    , m_customEdit(nullptr)
    , m_discountName(nullptr)
    , m_discountValue(nullptr)
    , m_totalLabel(nullptr)
{
    setObjectName("discount-dialog");
    m_presets.append({-1, QString(), QString(), 0.0});
    loadPresets();
    setupUI();
    updatePreview();
}

DiscountDialog::~DiscountDialog() {}

void DiscountDialog::setupUI()
{
    // Preset chips wrap onto new rows; a long list scrolls.
    auto* chipArea = new QWidget;
    chipArea->setObjectName("discountChips");
    auto* rows = new QVBoxLayout(chipArea);
    rows->setContentsMargins(0, 0, 0, 0);
    rows->setSpacing(ChipGap);
    QHBoxLayout* row = nullptr;
    int used = 0;
    for (int i = 0; i < m_presets.size(); ++i) {
        const Preset& preset = m_presets.at(i);
        auto* chip = new QPushButton(preset.id < 0 ? QString("None") : chipText(preset), chipArea);
        chip->setObjectName("discountChip");
        chip->setCheckable(true);
        chip->setAutoDefault(false);
        chip->setCursor(Qt::PointingHandCursor);
        m_presetGroup->addButton(chip, i);
        chip->ensurePolished();
        const int width = chip->sizeHint().width();
        if (!row || used + width > ChipAreaWidth) {
            row = new QHBoxLayout;
            row->setSpacing(ChipGap);
            row->addStretch(1);
            rows->addLayout(row);
            used = 0;
        }
        row->insertWidget(row->count() - 1, chip);
        used += width + ChipGap;
    }
    m_presetGroup->button(0)->setChecked(true);

    auto* scroll = new QScrollArea(this);
    scroll->setObjectName("discountChipScroll");
    scroll->setWidget(chipArea);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setFixedHeight(qMin(chipArea->sizeHint().height(), MaxChipAreaHeight));
    addField(tr("Presets"), scroll);

    m_customEdit = new QLineEdit(this);
    m_customEdit->setObjectName("discount-dialog-input");
    m_customEdit->setPlaceholderText(tr("5  or  10%"));
    addField(tr("Or a custom discount"), m_customEdit);

    // The result, on the right
    QVBoxLayout* side = addSidePanel(tr("The result"));
    auto* card = new QFrame(this);
    card->setObjectName("discountResult");
    auto* lines = new QVBoxLayout(card);
    lines->setContentsMargins(14, 12, 14, 12);
    lines->setSpacing(6);

    auto addLine = [&](QLabel*& name, QLabel*& value) {
        auto* line = new QHBoxLayout;
        name = new QLabel(card);
        name->setObjectName("discountLineName");
        value = new QLabel(card);
        value->setObjectName("discountLineValue");
        value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        line->addWidget(name, 1);
        line->addWidget(value);
        lines->addLayout(line);
    };
    QLabel* subtotalName = nullptr;
    QLabel* subtotalValue = nullptr;
    addLine(subtotalName, subtotalValue);
    subtotalName->setText(tr("Cart subtotal"));
    subtotalValue->setText(AppHelpers::formatAmount(m_subtotal));
    addLine(m_discountName, m_discountValue);
    m_discountValue->setProperty("amber", true);

    auto* divider = new QFrame(card);
    divider->setObjectName("modalRowLine");
    divider->setFixedHeight(1);
    lines->addSpacing(4);
    lines->addWidget(divider);
    lines->addSpacing(4);

    auto* paysCaption = new QLabel(tr("Customer pays"), card);
    paysCaption->setObjectName("discountLineName");
    m_totalLabel = new QLabel(card);
    m_totalLabel->setObjectName("discountTotal");
    auto* currency = new QLabel(AppConstants::CURRENCY, card);
    currency->setObjectName("discountLineName");
    lines->addWidget(paysCaption);
    lines->addWidget(m_totalLabel);
    lines->addWidget(currency);
    side->addWidget(card);

    auto* hint = new QLabel(tr("Updates as you pick a preset or type an amount."), this);
    hint->setObjectName("modalSubtitle");
    hint->setWordWrap(true);
    side->addWidget(hint);
    side->addStretch(1);

    QPushButton* okBtn = addFooter(tr("Apply Discount"), "check", "add");
    connect(okBtn, &QPushButton::clicked, this, &DiscountDialog::onOkClicked);
    connect(m_customEdit, &QLineEdit::textChanged, this, &DiscountDialog::onCustomInputChanged);
    connect(m_presetGroup, &QButtonGroup::idClicked, this, &DiscountDialog::onPresetChanged);
}

QString DiscountDialog::chipText(const Preset& preset)
{
    return preset.type == "percentage"
               ? QString("%1  %2%").arg(preset.name).arg(preset.value, 0, 'g', 6)
               : QString("%1  %2").arg(preset.name, AppHelpers::formatAmount(preset.value));
}

void DiscountDialog::loadPresets()
{
    if (!m_api) return;

    const QList<DataRow> rows = m_api->activeDiscounts();
    if (!m_api->ok()) {
        qWarning() << "DiscountDialog: failed to load presets:" << m_api->lastError();
        return;
    }

    for (const DataRow& row : rows) {
        m_presets.append({row.value("id").toInt(), row.value("name").toString(),
                          row.value("type").toString(), row.value("value").toDouble()});
    }
}

const DiscountDialog::Preset& DiscountDialog::selectedPreset() const
{
    return m_presets.at(qMax(0, m_presetGroup->checkedId()));
}

void DiscountDialog::onCustomInputChanged(const QString&)
{
    // When user types in custom field, clear preset selection
    if (!m_customEdit->text().trimmed().isEmpty())
        m_presetGroup->button(0)->setChecked(true);  // "None"

    updatePreview();
}

void DiscountDialog::onPresetChanged(int)
{
    // When user picks a preset, clear custom field
    if (selectedPreset().id != -1)
        m_customEdit->clear();

    updatePreview();
}

void DiscountDialog::updatePreview()
{
    DiscountItem preview;
    bool valid = false;

    const QString customText = m_customEdit->text().trimmed();

    if (!customText.isEmpty()) {
        // Custom input takes priority
        const bool isPercent = customText.endsWith('%');
        const QString numStr = isPercent ? customText.chopped(1).trimmed() : customText;

        bool ok = false;
        const double val = numStr.toDouble(&ok);

        if (ok && val > 0 && (!isPercent || val <= 100)) {
            preview.type  = isPercent ? "percentage" : "amount";
            preview.value = val;
            preview.name  = isPercent ? tr("Custom (%1%)").arg(val) : tr("Custom");
            valid = true;
        }
    } else if (selectedPreset().id != -1) {
        const Preset& preset = selectedPreset();
        preview.presetId = preset.id;
        preview.type     = preset.type;
        preview.value    = preset.value;
        preview.name     = preset.type == "percentage"
                               ? QString("%1 (%2%)").arg(preset.name).arg(preset.value, 0, 'g', 6)
                               : preset.name;
        valid = true;
    }

    const double amount = valid ? DiscountHandler::calculateAmount(preview, m_subtotal) : 0.0;
    const bool tooHigh = !valid && customText.endsWith('%')
                         && customText.chopped(1).trimmed().toDouble() > 100;
    m_discountName->setText(valid ? preview.name : tooHigh ? tr("More than 100%") : tr("No discount yet"));
    m_discountValue->setText(valid ? QString("− %1").arg(AppHelpers::formatAmount(amount)) : QString("-"));
    m_totalLabel->setText(AppHelpers::formatAmount(m_subtotal - amount));
}

void DiscountDialog::onOkClicked()
{
    const QString customText = m_customEdit->text().trimmed();

    if (!customText.isEmpty()) {
        const bool isPercent = customText.endsWith('%');
        const QString numStr = isPercent ? customText.chopped(1).trimmed() : customText;

        bool ok = false;
        const double val = numStr.toDouble(&ok);

        if (!ok || val <= 0) {
            AppHelpers::showWarning(this, tr("Please enter a valid discount value."));
            m_customEdit->setFocus();
            return;
        }
        if (isPercent && val > 100) {
            AppHelpers::showWarning(this, tr("A discount can't be more than 100%."));
            m_customEdit->setFocus();
            return;
        }

        m_result.type     = isPercent ? "percentage" : "amount";
        m_result.value    = val;
        m_result.name     = isPercent
                            ? tr("Custom (%1%)").arg(val)
                            : tr("Custom (%1)").arg(AppHelpers::formatPrice(val));
        m_result.presetId = -1;
        accept();
        return;
    }

    // No custom input — check preset
    const Preset& preset = selectedPreset();
    if (preset.id == -1) {
        AppHelpers::showWarning(this, tr("Please select a preset or enter a custom discount."));
        return;
    }

    m_result.presetId = preset.id;
    m_result.type     = preset.type;
    m_result.value    = preset.value;
    m_result.name     = preset.name;
    accept();
}

DiscountItem DiscountDialog::result() const
{
    return m_result;
}
