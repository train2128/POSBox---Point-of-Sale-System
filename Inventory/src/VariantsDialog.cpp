#include "VariantsDialog.h"
#include "ui_Variantsdialog.h"
#include "InventoryConstants.h"
#include "DatabaseManager.h"
#include "QueryRepository.h"
#include "StyleManager.h"
#include "Prefixes.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QIcon>
#include <QIntValidator>
#include <QRegularExpression>
#include <QStyle>
#include <QSizePolicy>
#include <QDebug>

// ===== HELPERS =====

static QString stripCurrency(const QString& text)
{
    QString result = text.trimmed();
    // Remove trailing currency code (e.g. " TND", " USD", etc.)
    static QRegularExpression rx("\\s+\\S+$");
    result.remove(rx);
    return result;
}

static void connectCurrencyFormat(QLineEdit* edit, QWidget* parent)
{
    QObject::connect(edit, &QLineEdit::editingFinished, [edit]() {
        bool ok;
        double val = stripCurrency(edit->text()).toDouble(&ok);
        if (ok && val > 0)
            edit->setText(AppHelpers::formatPrice(val));
    });
}

static QString currencyText(double val)
{
    return val > 0 ? AppHelpers::formatPrice(val) : QString();
}

// ===== CONSTRUCTOR =====

VariantsDialog::VariantsDialog(DatabaseManager* db,
                               const QList<PendingVariant>& pending,
                               int productId,
                               bool isService,
                               QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::VarDialog)
    , m_db(db)
    , m_productId(productId)
    , m_isService(isService)
{
    ui->setupUi(this);
    setupUI();
    setupConnections();

    // Register the static .ui row as m_rows[0] (locked, cannot be deleted)
    registerStaticRow();

    if (m_isService) {
        ui->varStockLabel->setEnabled(false);
        ui->varStockEdit->setEnabled(false);
        ui->varStockEdit->setText("0");
    } else {
        ui->varStockLabel->setText(tr("Stock"));
    }

    if (!pending.isEmpty()) {
        populateFromPending(pending);
    } else if (m_productId > 0) {
        loadExistingVariants();
    }
    // Add mode with no pending data: static row is empty and ready to fill
}

VariantsDialog::~VariantsDialog()
{
    delete ui;
}

// ===== SETUP =====

void VariantsDialog::setupUI()
{
    setWindowTitle(tr("Manage Variants"));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    ui->varNameLabel->setText(tr("Name"));
    ui->varCostLabel->setText(tr("Cost"));
    ui->varPriceLabel->setText(tr("Price"));
    ui->varAddBtn->setText(tr("+ Add Variant"));

    ui->varbuttonBox->button(QDialogButtonBox::Ok)->setText(tr("Save"));
    ui->varbuttonBox->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
}

void VariantsDialog::setupConnections()
{
    connect(ui->varAddBtn, &QPushButton::clicked,
            this, &VariantsDialog::onAddVariantRow);
    connect(ui->varbuttonBox, &QDialogButtonBox::accepted,
            this, &VariantsDialog::onAccepted);
    connect(ui->varbuttonBox, &QDialogButtonBox::rejected,
            this, &VariantsDialog::onRejected);
}

// ===== ROW MANAGEMENT =====

void VariantsDialog::registerStaticRow()
{
    VariantRow row;
    row.variantId  = -1;
    row.nameLabel  = ui->varNameLabel;
    row.nameEdit   = ui->varNameEdit;
    row.costLabel  = ui->varCostLabel;
    row.costEdit   = ui->varCostEdit;
    row.priceLabel = ui->varPriceLabel;
    row.priceEdit  = ui->varPriceEdit;
    row.stockLabel = ui->varStockLabel;
    row.stockEdit  = ui->varStockEdit;
    row.deleteBtn  = ui->deleteVarBtn;
    row.layout     = nullptr; // owned by the .ui

    ui->varNameEdit->setPlaceholderText(tr("Variant name"));
    ui->varCostEdit->setPlaceholderText(QString("0.%1").arg(QString(AppConstants::PRICE_DECIMALS, '0')));
    ui->varPriceEdit->setPlaceholderText(QString("0.%1").arg(QString(AppConstants::PRICE_DECIMALS, '0')));
    ui->varStockEdit->setPlaceholderText("0");

    connectCurrencyFormat(ui->varCostEdit, this);
    connectCurrencyFormat(ui->varPriceEdit, this);

    styleDeleteButton(ui->deleteVarBtn, false);

    // Apply matching layout settings to the static .ui row
    ui->horizontalLayout->setContentsMargins(0, 0, 0, 0);
    ui->horizontalLayout->setSpacing(6);
    ui->horizontalLayout->setStretch(0, 0); // varNameLabel
    ui->horizontalLayout->setStretch(1, 1); // varNameEdit
    ui->horizontalLayout->setStretch(2, 0); // varCostLabel
    ui->horizontalLayout->setStretch(3, 1); // varCostEdit
    ui->horizontalLayout->setStretch(4, 0); // varPriceLabel
    ui->horizontalLayout->setStretch(5, 1); // varPriceEdit
    ui->horizontalLayout->setStretch(6, 0); // varStockLabel
    ui->horizontalLayout->setStretch(7, 1); // varStockEdit
    ui->horizontalLayout->setStretch(8, 0); // deleteVarBtn

    m_rows.append(row);
}

void VariantsDialog::styleDeleteButton(QPushButton* btn, bool enabled)
{
    btn->setText("");
    btn->setProperty("btnStyle", "variantDelete");
    btn->setEnabled(enabled);
    btn->setIcon(QIcon(StyleManager::iconPath("delete")));
    btn->setIconSize(QSize(16, 16));
    btn->style()->unpolish(btn);
    btn->style()->polish(btn);
    btn->update();
}

void VariantsDialog::loadExistingVariants()
{
    if (!m_db || m_productId <= 0) return;

    QSqlQuery query(m_db->database());
    query.prepare(Queries::VARIANT_SELECT_BY_PRODUCT);
    query.addBindValue(m_productId);

    if (!query.exec()) {
        qWarning() << "Failed to load variants:" << query.lastError().text();
        return;
    }

    bool first = true;
    while (query.next()) {
        int     id    = query.value("id").toInt();
        QString name  = query.value("variant_name").toString();
        double  cost  = query.value("cost").toDouble();
        double  price = query.value("price").toDouble();
        int     stock = query.value("stock").toInt();

        if (first) {
            m_rows[0].variantId = id;
            ui->varNameEdit->setText(name);
            ui->varCostEdit->setText(currencyText(cost));
            ui->varPriceEdit->setText(currencyText(price));
            ui->varStockEdit->setText(QString::number(stock));
            first = false;
        } else {
            addVariantRow(id, name, cost, price, stock);
        }
    }
}

void VariantsDialog::populateFromPending(const QList<PendingVariant>& pending)
{
    if (pending.isEmpty()) return;

    const PendingVariant& first = pending[0];
    m_rows[0].variantId = first.variantId;
    ui->varNameEdit->setText(first.name);
    ui->varCostEdit->setText(currencyText(first.cost));
    ui->varPriceEdit->setText(currencyText(first.price));
    ui->varStockEdit->setText(QString::number(first.stock));

    for (int i = 1; i < pending.size(); ++i) {
        const PendingVariant& pv = pending[i];
        addVariantRow(pv.variantId, pv.name, pv.cost, pv.price, pv.stock);
    }
}

void VariantsDialog::addVariantRow(int variantId, const QString& name, double cost, double price, int stock)
{
    VariantRow row;
    row.variantId = variantId;

    row.nameLabel  = new QLabel(tr("Name"),  this);
    row.costLabel  = new QLabel(tr("Cost"),  this);
    row.priceLabel = new QLabel(tr("Price"), this);
    row.stockLabel = new QLabel(tr("Stock"), this);

    row.nameEdit = new QLineEdit(name, this);
    row.nameEdit->setPlaceholderText(tr("Variant name"));

    row.costEdit = new QLineEdit(currencyText(cost), this);
    row.costEdit->setPlaceholderText(QString("0.%1").arg(QString(AppConstants::PRICE_DECIMALS, '0')));
    connectCurrencyFormat(row.costEdit, this);

    row.priceEdit = new QLineEdit(currencyText(price), this);
    row.priceEdit->setPlaceholderText(QString("0.%1").arg(QString(AppConstants::PRICE_DECIMALS, '0')));
    connectCurrencyFormat(row.priceEdit, this);

    row.stockEdit = new QLineEdit(QString::number(stock), this);
    row.stockEdit->setPlaceholderText("0");
    row.stockEdit->setValidator(new QIntValidator(0, 999999, this));
    if (m_isService) {
        row.stockLabel->setEnabled(false);
        row.stockEdit->setEnabled(false);
        row.stockEdit->setText("0");
    }

    row.deleteBtn = new QPushButton(this);
    row.deleteBtn->setToolTip(tr("Remove variant"));
    styleDeleteButton(row.deleteBtn, true);

    connect(row.deleteBtn, &QPushButton::clicked, this, &VariantsDialog::onDeleteVariantRow);

    row.layout = new QHBoxLayout();
    row.layout->setContentsMargins(0, 0, 0, 0);
    row.layout->setSpacing(6);
    row.layout->addWidget(row.nameLabel,  0); // stretch 0
    row.layout->addWidget(row.nameEdit,   1); // stretch 1
    row.layout->addWidget(row.costLabel,  0); // stretch 0
    row.layout->addWidget(row.costEdit,   1); // stretch 1
    row.layout->addWidget(row.priceLabel, 0); // stretch 0
    row.layout->addWidget(row.priceEdit,  1); // stretch 1
    row.layout->addWidget(row.stockLabel, 0); // stretch 0
    row.layout->addWidget(row.stockEdit,  1); // stretch 1
    row.layout->addWidget(row.deleteBtn,  0); // stretch 0

    // Insert after all existing rows, before varAddBtn
    int insertIndex = m_rows.size();
    ui->verticalLayout->insertLayout(insertIndex, row.layout);

    m_rows.append(row);
}

void VariantsDialog::onAddVariantRow()
{
    addVariantRow();
}

void VariantsDialog::onDeleteVariantRow()
{
    QPushButton* btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;

    // Start from index 1 — index 0 is the locked static row
    for (int i = 1; i < m_rows.size(); ++i) {
        if (m_rows[i].deleteBtn == btn) {
            removeVariantRow(i);
            return;
        }
    }
}

void VariantsDialog::removeVariantRow(int index)
{
    if (index <= 0 || index >= m_rows.size()) return; // 0 is locked

    VariantRow& row = m_rows[index];

    QLayoutItem* item;
    while ((item = row.layout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    ui->verticalLayout->removeItem(row.layout);
    delete row.layout;

    m_rows.removeAt(index);
}

// ===== ACCEPT / REJECT =====

void VariantsDialog::onAccepted()
{
    if (validateAndCollect()) {
        accept();
    }
}

void VariantsDialog::onRejected()
{
    reject();
}

bool VariantsDialog::validateAndCollect()
{
    if (m_rows.isEmpty()) {
        AppHelpers::showWarning(this, tr(InventoryConstants::MSG_VARIANT_NO_VARIANTS));
        return false;
    }

    QList<PendingVariant> result;

    for (int i = 0; i < m_rows.size(); ++i) {
        const VariantRow& row = m_rows[i];

        QString name = row.nameEdit->text().trimmed();
        if (name.isEmpty()) {
            AppHelpers::showWarning(this, QString(tr("Variant %1: %2"))
                                        .arg(i + 1)
                                        .arg(tr(InventoryConstants::MSG_VARIANT_NAME_EMPTY)));
            return false;
        }

        bool costOk = false, priceOk = false;
        double cost  = stripCurrency(row.costEdit->text()).toDouble(&costOk);
        double price = stripCurrency(row.priceEdit->text()).toDouble(&priceOk);

        if (!costOk) {
            AppHelpers::showWarning(this, QString(tr("Variant %1: %2"))
                                        .arg(i + 1)
                                        .arg(tr(InventoryConstants::MSG_VARIANT_COST_INVALID)));
            return false;
        }
        if (!priceOk) {
            AppHelpers::showWarning(this, QString(tr("Variant %1: %2"))
                                        .arg(i + 1)
                                        .arg(tr(InventoryConstants::MSG_VARIANT_PRICE_INVALID)));
            return false;
        }

        int stock = row.stockEdit ? row.stockEdit->text().toInt() : 0;
        result.append(PendingVariant(row.variantId, name, cost, price, stock));
    }

    m_pendingVariants = result;
    qDebug() << "VariantsDialog: collected" << m_pendingVariants.size() << "variants";
    return true;
}
