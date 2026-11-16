#include "VariantPickerDialog.h"
#include "LocalApi.h"
#include "Prefixes.h"
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QDebug>

namespace {
constexpr int RowHeight = 62;
constexpr int MaxRowsShown = 6;
}

VariantPickerDialog::VariantPickerDialog(const QString& productName,
                                         int productId,
                                         LocalApi* api,
                                         bool isService,
                                         QWidget* parent)
    : ModernDialog(Look::Band, "box", Tone::Accent, productName, tr("Choose an option"), 420, parent)
    , m_api(api)
    , m_isService(isService)
{
    setObjectName("variant-picker-dialog");
    loadVariants(productId, isService);
    addCancelLink();
}

// Each option is one large row; clicking it chooses straight away.
void VariantPickerDialog::loadVariants(int productId, bool isService)
{
    if (!m_api || productId <= 0) return;

    m_isService = isService;

    const QList<DataRow> options = m_api->variants(productId);
    if (!m_api->ok()) {
        qWarning() << "VariantPickerDialog: failed to load variants:" << m_api->lastError();
        return;
    }

    auto* listWidget = new QWidget;
    listWidget->setObjectName("variantPickerRows");
    auto* list = new QVBoxLayout(listWidget);
    list->setContentsMargins(0, 0, 0, 0);
    list->setSpacing(8);

    QPushButton* first = nullptr;
    for (const DataRow& row : options) {
        VariantOption opt;
        opt.id    = row.value("id").toInt();
        opt.name  = row.value("variant_name").toString();
        opt.price = row.value("price").toDouble();
        opt.cost  = row.value("cost").toDouble();
        opt.stock = row.value("stock").toInt();
        m_variants.append(opt);

        auto* button = new QPushButton(listWidget);
        button->setObjectName("modalChoice");
        button->setCursor(Qt::PointingHandCursor);
        const bool available = isService || opt.stock > 0;
        button->setEnabled(available);

        auto* layout = new QHBoxLayout(button);
        layout->setContentsMargins(14, 8, 14, 8);
        auto* name = new QLabel(opt.name, button);
        name->setObjectName("modalChoiceTitle");
        auto* right = new QVBoxLayout;
        right->setSpacing(0);
        auto* price = new QLabel(AppHelpers::formatPrice(opt.price), button);
        price->setObjectName("modalChoiceTitle");
        price->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        right->addWidget(price);
        for (QLabel* label : {name, price})
            label->setAttribute(Qt::WA_TransparentForMouseEvents);
        if (!isService) {
            auto* stock = new QLabel(opt.stock <= 0 ? tr("Out of stock") : tr("%1 left").arg(opt.stock), button);
            stock->setObjectName("variantStock");
            stock->setProperty("level", opt.stock <= 0 ? "out" : opt.stock <= 3 ? "low" : "ok");
            stock->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            stock->setAttribute(Qt::WA_TransparentForMouseEvents);
            right->addWidget(stock);
        }
        layout->addWidget(name, 1);
        layout->addLayout(right);
        list->addWidget(button);

        const VariantOption chosen = opt;
        connect(button, &QPushButton::clicked, this, [this, chosen]() {
            m_selected = chosen;
            accept();
        });
        if (available && !first)
            first = button;
    }
    if (first) {
        first->setDefault(true);
        first->setFocus();
    }

    auto* scroll = new QScrollArea(this);
    scroll->setObjectName("variantPickerScroll");
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidgetResizable(true);
    scroll->setWidget(listWidget);
    const int rows = qMin(int(options.size()), MaxRowsShown);
    scroll->setFixedHeight(rows * RowHeight + qMax(0, rows - 1) * list->spacing());
    body()->addWidget(scroll);
}

VariantOption VariantPickerDialog::selectedVariant() const
{
    return m_selected;
}
