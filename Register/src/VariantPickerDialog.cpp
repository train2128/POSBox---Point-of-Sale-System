#include "VariantPickerDialog.h"
#include "DatabaseManager.h"
#include "QueryRepository.h"
#include "Prefixes.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

VariantPickerDialog::VariantPickerDialog(const QString& productName,
                                         int productId,
                                         DatabaseManager* db,
                                         bool isService,
                                         QWidget* parent)
    : QDialog(parent)
    , m_db(db)
    , m_list(nullptr)
    , m_isService(isService)
{
    setObjectName("variant-picker-dialog");
    setWindowTitle(tr("Select Variant"));
    setModal(true);
    setFixedWidth(320);

    setupUI(productName);
    loadVariants(productId, isService);
}

VariantPickerDialog::~VariantPickerDialog() {}

void VariantPickerDialog::setupUI(const QString& productName)
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    // Product name header
    QLabel* titleLabel = new QLabel(productName, this);
    titleLabel->setObjectName("variant-picker-title");
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);

    // Variant list
    m_list = new QListWidget(this);
    m_list->setObjectName("variant-picker-list");
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    mainLayout->addWidget(m_list);

    // OK / Cancel buttons
    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(8);

    QPushButton* okBtn = new QPushButton(tr("OK"), this);
    okBtn->setObjectName("variant-picker-ok");
    okBtn->setProperty("btnStyle", "add");
    okBtn->style()->unpolish(okBtn);
    okBtn->style()->polish(okBtn);

    QPushButton* cancelBtn = new QPushButton(tr("Cancel"), this);
    cancelBtn->setObjectName("variant-picker-cancel");
    cancelBtn->setProperty("btnStyle", "delete");
    cancelBtn->style()->unpolish(cancelBtn);
    cancelBtn->style()->polish(cancelBtn);

    btnLayout->addWidget(okBtn);
    btnLayout->addWidget(cancelBtn);
    mainLayout->addLayout(btnLayout);

    connect(okBtn,     &QPushButton::clicked, this, &VariantPickerDialog::onOkClicked);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_list,    &QListWidget::itemDoubleClicked, this, &VariantPickerDialog::onItemDoubleClicked);
}

void VariantPickerDialog::loadVariants(int productId, bool isService)
{
    if (!m_db || productId <= 0) return;

    m_isService = isService;

    QSqlQuery query(m_db->database());
    query.prepare(Queries::VARIANT_SELECT_BY_PRODUCT);
    query.addBindValue(productId);

    if (!query.exec()) {
        qWarning() << "VariantPickerDialog: failed to load variants:" << query.lastError().text();
        return;
    }

    while (query.next()) {
        VariantOption opt;
        opt.id    = query.value("id").toInt();
        opt.name  = query.value("variant_name").toString();
        opt.price = query.value("price").toDouble();
        opt.cost  = query.value("cost").toDouble();
        opt.stock = query.value("stock").toInt();

        m_variants.append(opt);

        QString label;
        if (isService) {
            label = QString("%1  —  %2")
                        .arg(opt.name)
                        .arg(AppHelpers::formatPrice(opt.price));
        } else {
            QString stockStr;
            if (opt.stock <= 0) {
                stockStr = tr(" (Out of Stock)");
            } else {
                stockStr = QString(tr(" (%1 left)")).arg(opt.stock);
            }
            label = QString("%1  —  %2%3")
                        .arg(opt.name)
                        .arg(AppHelpers::formatPrice(opt.price))
                        .arg(stockStr);
        }

        QListWidgetItem* item = new QListWidgetItem(label, m_list);
        item->setData(Qt::UserRole, opt.id);

        if (!isService && opt.stock <= 0) {
            item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
            item->setForeground(Qt::gray);
        }
    }

    if (m_list->count() > 0)
        m_list->setCurrentRow(0);

    // Resize height to fit list content (max 6 rows)
    int rowHeight = m_list->sizeHintForRow(0);
    int rows      = qMin(m_list->count(), 6);
    m_list->setFixedHeight(rowHeight * rows + 8);
    adjustSize();
}

VariantOption VariantPickerDialog::selectedVariant() const
{
    return m_selected;
}

void VariantPickerDialog::onOkClicked()
{
    QListWidgetItem* item = m_list->currentItem();
    if (!item) return;

    int selectedId = item->data(Qt::UserRole).toInt();
    for (const VariantOption& opt : m_variants) {
        if (opt.id == selectedId) {
            m_selected = opt;
            accept();
            return;
        }
    }
}

void VariantPickerDialog::onItemDoubleClicked()
{
    onOkClicked();
}
