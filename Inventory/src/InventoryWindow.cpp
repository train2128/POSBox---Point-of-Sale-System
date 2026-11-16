#include <QCoreApplication>
#include "InventoryWindow.h"
#include "Animations.h"
#include "ui_Invenorywindow.h"
#include "InventoryUIMap.h"
#include "InventoryConstants.h"
#include "ProductDialog.h"
#include "UserSession.h"
#include "LocalApi.h"
#include "LocalImages.h"
#include "ChangeNotifier.h"
#include "AccessControl.h"
#include "Permissions.h"
#include "StyleManager.h"
#include "LogoutManager.h"
#include "NotificationManager.h"
#include <QApplication>
#include "NotificationBell.h"
#include "NotificationActions.h"
#include "Prefixes.h"
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QStyleOption>
#include <QVariantAnimation>
#include "Animations.h"
#include "LineIcons.h"
#include "ModernDialog.h"
#include "ToggleSwitch.h"
#include <QStyle>
#include <QFileInfo>
#include <QVBoxLayout>
#include <QResizeEvent>
#include <QColor>
#include <QImage>
#include <QPixmap>
#include <QDialog>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QDateEdit>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QFrame>
#include <QSpacerItem>
#include <QSizePolicy>
#include <QDebug>

namespace {

constexpr int BadgeColorRole = Qt::UserRole + 1;
constexpr int StockKindRole = Qt::UserRole + 2;
constexpr int StockQuantityRole = Qt::UserRole + 3;
constexpr int ProductImagePathRole = Qt::UserRole + 4;
constexpr int ProductImagePixmapRole = Qt::UserRole + 5;
constexpr int ProductImageSize = 44;

// The holder of the tab buttons: it paints the pill, which slides from tab to tab.
class InventoryTabBar final : public QWidget
{
public:
    explicit InventoryTabBar(QWidget* parent = nullptr)
        : QWidget(parent), m_slide(new QVariantAnimation(this))
    {
        m_slide->setDuration(PillSlideMs);
        m_slide->setEasingCurve(QEasingCurve::OutCubic);
        connect(m_slide, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
            m_pill = value.toRectF();
            update();
            for (QWidget* child : findChildren<QWidget*>())
                child->update();
        });
    }

    void watchTab(QWidget* tab) { tab->installEventFilter(this); }

    void setActive(QWidget* tab)
    {
        const QRectF target = pillFor(tab);
        const bool animate = isVisible() && Animations::enabled() && !m_pill.isNull() && tab != m_active;
        m_active = tab;
        m_slide->stop();
        if (animate) {
            m_slide->setStartValue(m_pill);
            m_slide->setEndValue(target);
            m_slide->start();
        } else {
            m_pill = target;
            update();
        }
    }

    // How much of a tab the pill covers right now, from 0 to 1.
    qreal coverage(const QWidget* tab) const
    {
        const QRectF overlap = m_pill.intersected(QRectF(tab->geometry()));
        return overlap.isEmpty() ? 0.0 : overlap.width() / qMax(1, tab->width());
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QStyleOption option;
        option.initFrom(this);
        QPainter painter(this);
        style()->drawPrimitive(QStyle::PE_Widget, &option, &painter, this);
        if (m_pill.isNull())
            return;
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(palette().color(QPalette::Highlight));
        painter.drawRoundedRect(m_pill, m_pill.height() / 2.0, m_pill.height() / 2.0);
    }

    bool eventFilter(QObject* watched, QEvent* event) override
    {
        // The layout moved the tabs (first show, resize, font change).
        if (watched == m_active && (event->type() == QEvent::Resize || event->type() == QEvent::Move)
            && m_slide->state() != QAbstractAnimation::Running) {
            m_pill = pillFor(m_active);
            update();
        }
        return QWidget::eventFilter(watched, event);
    }

private:
    static constexpr int PillSlideMs = 260;

    static QRectF pillFor(const QWidget* tab)
    {
        return QRectF(tab->geometry()).adjusted(1, 1, -1, -1);
    }

    QVariantAnimation* m_slide;
    QRectF m_pill;
    QWidget* m_active = nullptr;
};

// A tab's text turns white as the pill slides under it.
class InventoryTabButton final : public QPushButton
{
public:
    using QPushButton::QPushButton;

protected:
    void paintEvent(QPaintEvent* event) override
    {
        const auto* bar = static_cast<const InventoryTabBar*>(parentWidget());
        const qreal covered = bar->coverage(this);
        if (covered <= 0.0) {
            QPushButton::paintEvent(event);
            return;
        }

        const QColor from = LineIcons::colors().muted;
        const qreal k = qMin(1.0, covered * 1.25);
        QPainter painter(this);
        painter.setPen(QColor::fromRgbF(from.redF() + (1.0 - from.redF()) * k,
                                        from.greenF() + (1.0 - from.greenF()) * k,
                                        from.blueF() + (1.0 - from.blueF()) * k));
        painter.setFont(font());
        painter.drawText(rect(), Qt::AlignCenter, text());
    }
};

// Add and Edit Category share one window.
bool categoryForm(QWidget* parent, bool editing, QString& name, QString& description)
{
    ModernDialog dlg(ModernDialog::Look::Card, "grid", ModernDialog::Tone::Accent,
                     editing ? QCoreApplication::translate("InventoryWindow", "Edit category") : QCoreApplication::translate("InventoryWindow", "Add category"),
                     QCoreApplication::translate("InventoryWindow", "Group products in Inventory and Register"), 420, parent);
    auto* nameEdit = new QLineEdit(name, &dlg);
    nameEdit->setPlaceholderText(QCoreApplication::translate("InventoryWindow", "Category name"));
    dlg.addField(QCoreApplication::translate("InventoryWindow", "Name"), nameEdit);
    auto* descEdit = new QLineEdit(description, &dlg);
    descEdit->setPlaceholderText(QCoreApplication::translate("InventoryWindow", "Optional"));
    dlg.addField(QCoreApplication::translate("InventoryWindow", "Description"), descEdit);

    QPushButton* ok = dlg.addFooter(editing ? QCoreApplication::translate("InventoryWindow", "Save") : QCoreApplication::translate("InventoryWindow", "Add Category"), editing ? "check" : "plus", "add");
    QObject::connect(ok, &QPushButton::clicked, &dlg, [&]() {
        if (nameEdit->text().trimmed().isEmpty()) {
            AppHelpers::showWarning(&dlg, AppHelpers::trMsg(InventoryConstants::MSG_CATEGORY_NAME_EMPTY));
            nameEdit->setFocus();
        } else {
            dlg.accept();
        }
    });
    nameEdit->setFocus();
    nameEdit->selectAll();

    if (dlg.exec() != QDialog::Accepted)
        return false;
    name = nameEdit->text().trimmed();
    description = descEdit->text().trimmed();
    return true;
}

struct DiscountValues {
    QString name;
    QString type = "percentage";
    double value = 0.0;
    bool active = true;
    QDate start;  // invalid = no start date
    QDate end;    // invalid = no end date
};

// A label with a switch, above a date field the switch turns on and off.
QVBoxLayout* optionalDate(QDialog* dlg, const QString& label, const QDate& date, ToggleSwitch*& onOff, QDateEdit*& edit)
{
    auto* column = new QVBoxLayout;
    column->setSpacing(5);
    auto* top = new QHBoxLayout;
    auto* caption = new QLabel(label, dlg);
    caption->setObjectName("modalFieldLabel");
    onOff = new ToggleSwitch(dlg);
    onOff->setChecked(date.isValid());
    top->addWidget(caption, 1);
    top->addWidget(onOff);
    edit = new QDateEdit(date.isValid() ? date : QDate::currentDate(), dlg);
    edit->setCalendarPopup(true);
    edit->setDisplayFormat("dd/MM/yyyy");
    edit->setEnabled(date.isValid());
    QObject::connect(onOff, &QCheckBox::toggled, edit, &QDateEdit::setEnabled);
    column->addLayout(top);
    column->addWidget(edit);
    return column;
}

// Add and Edit Discount share one window.
bool discountForm(QWidget* parent, bool editing, DiscountValues& values)
{
    ModernDialog dlg(ModernDialog::Look::Card, "tag", ModernDialog::Tone::Amber,
                     editing ? QCoreApplication::translate("InventoryWindow", "Edit discount") : QCoreApplication::translate("InventoryWindow", "Add discount"),
                     QCoreApplication::translate("InventoryWindow", "Saved here, applied at the Register"), 470, parent);
    auto* nameEdit = new QLineEdit(values.name, &dlg);
    nameEdit->setPlaceholderText(QCoreApplication::translate("InventoryWindow", "Discount name"));
    dlg.addField(QCoreApplication::translate("InventoryWindow", "Name"), nameEdit);

    auto* typeCombo = new QComboBox(&dlg);
    typeCombo->addItem(QCoreApplication::translate("InventoryWindow", "Percentage (%)"), "percentage");
    typeCombo->addItem(QCoreApplication::translate("InventoryWindow", "Fixed Amount"), "amount");
    typeCombo->setCurrentIndex(values.type == "percentage" ? 0 : 1);
    auto* valueSpin = new QDoubleSpinBox(&dlg);
    valueSpin->setMinimum(0.0);
    valueSpin->setDecimals(AppConstants::PRICE_DECIMALS);
    // The unit beside the value follows the type.
    const auto updateUnit = [typeCombo, valueSpin]() {
        const bool percent = typeCombo->currentData().toString() == "percentage";
        valueSpin->setMaximum(percent ? 100.0 : 9999999.0);
        valueSpin->setSuffix(percent ? QString(" %") : QString(" %1").arg(AppConstants::CURRENCY));
    };
    updateUnit();
    valueSpin->setValue(values.value);
    QObject::connect(typeCombo, &QComboBox::currentIndexChanged, valueSpin, updateUnit);
    dlg.addFieldPair(QCoreApplication::translate("InventoryWindow", "Type"), typeCombo, QCoreApplication::translate("InventoryWindow", "Value"), valueSpin);

    ToggleSwitch* startSwitch = nullptr;
    ToggleSwitch* endSwitch = nullptr;
    QDateEdit* startEdit = nullptr;
    QDateEdit* endEdit = nullptr;
    auto* dates = new QHBoxLayout;
    dates->setSpacing(10);
    dates->addLayout(optionalDate(&dlg, QCoreApplication::translate("InventoryWindow", "Start date"), values.start, startSwitch, startEdit), 1);
    dates->addLayout(optionalDate(&dlg, QCoreApplication::translate("InventoryWindow", "End date"), values.end, endSwitch, endEdit), 1);
    dlg.body()->addLayout(dates);

    auto* activeRow = new QHBoxLayout;
    auto* activeLabel = new QLabel(QCoreApplication::translate("InventoryWindow", "Active"), &dlg);
    activeLabel->setObjectName("modalSwitchLabel");
    auto* activeSwitch = new ToggleSwitch(&dlg);
    activeSwitch->setChecked(values.active);
    activeRow->addWidget(activeLabel, 1);
    activeRow->addWidget(activeSwitch);
    dlg.body()->addLayout(activeRow);

    QPushButton* ok = dlg.addFooter(editing ? QCoreApplication::translate("InventoryWindow", "Save") : QCoreApplication::translate("InventoryWindow", "Add Discount"), editing ? "check" : "plus", "add");
    QObject::connect(ok, &QPushButton::clicked, &dlg, [&]() {
        if (nameEdit->text().trimmed().isEmpty()) {
            AppHelpers::showWarning(&dlg, AppHelpers::trMsg(InventoryConstants::MSG_DISCOUNT_NAME_EMPTY));
            nameEdit->setFocus();
        } else {
            dlg.accept();
        }
    });
    nameEdit->setFocus();
    nameEdit->selectAll();

    if (dlg.exec() != QDialog::Accepted)
        return false;
    values.name = nameEdit->text().trimmed();
    values.type = typeCombo->currentData().toString();
    values.value = valueSpin->value();
    values.active = activeSwitch->isChecked();
    values.start = startSwitch->isChecked() ? startEdit->date() : QDate();
    values.end = endSwitch->isChecked() ? endEdit->date() : QDate();
    return true;
}

QVariant dateValue(const QDate& date)
{
    return date.isValid() ? QVariant(date.toString("yyyy-MM-dd")) : QVariant();
}

class InventoryCellDelegate final : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        const bool badgeCell =
            index.column() == InventoryConstants::COL_PRODUCT_BADGE
            && index.data(BadgeColorRole).canConvert<QColor>();
        const QPixmap productImage = index.data(ProductImagePixmapRole).value<QPixmap>();
        const QString stockKind = index.data(StockKindRole).toString();
        const bool stockCell = index.column() == InventoryConstants::COL_PRODUCT_STOCK
                               && !stockKind.isEmpty();
        if (!badgeCell && !stockCell) {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }

        QStyleOptionViewItem cell(option);
        initStyleOption(&cell, index);
        const QString text = stockCell
            ? stockDisplayText(stockKind, index.data(StockQuantityRole).toInt())
            : index.data(Qt::DisplayRole).toString();
        cell.text.clear();
        const QStyle* style = cell.widget ? cell.widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &cell, painter, cell.widget);

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        const QRect bounds = cell.rect.adjusted(4, 4, -4, -4);
        if (badgeCell) {
            const QSize badgeSize(ProductImageSize, ProductImageSize);
            const QRect badgeRect(QPoint(bounds.center().x() - badgeSize.width() / 2,
                                         bounds.center().y() - badgeSize.height() / 2),
                                  badgeSize);
            if (!productImage.isNull()) {
                painter->save();
                QPainterPath clipPath;
                clipPath.addRoundedRect(badgeRect, 9, 9);
                painter->setClipPath(clipPath);
                const QPixmap scaled = productImage.scaled(
                    badgeRect.size(), Qt::KeepAspectRatioByExpanding,
                    Qt::SmoothTransformation);
                const QRect source((scaled.width() - badgeRect.width()) / 2,
                                   (scaled.height() - badgeRect.height()) / 2,
                                   badgeRect.width(), badgeRect.height());
                painter->drawPixmap(badgeRect, scaled, source);
                painter->restore();
            } else {
                painter->setPen(Qt::NoPen);
                painter->setBrush(index.data(BadgeColorRole).value<QColor>());
                painter->drawRoundedRect(badgeRect, 9, 9);
                painter->setPen(Qt::white);
                QFont badgeFont = cell.font;
                badgeFont.setBold(true);
                painter->setFont(badgeFont);
                painter->drawText(badgeRect, Qt::AlignCenter, text);
            }
        } else {
            const QFontMetrics metrics(cell.font);
            const int chipWidth = qMin(bounds.width(),
                                       metrics.horizontalAdvance(text) + 24);
            const int chipHeight = qMin(bounds.height(), 32);
            const QRect chipRect(bounds.center().x() - chipWidth / 2,
                                 bounds.center().y() - chipHeight / 2,
                                 chipWidth, chipHeight);

            QColor accent;
            if (stockKind == QLatin1String("out")) {
                accent = QColor("#E74C3C");
            } else if (stockKind == QLatin1String("low")) {
                accent = QColor("#C8A000");
            } else if (stockKind == QLatin1String("variants")) {
                accent = QColor("#4A90E2");
            } else if (stockKind == QLatin1String("service")) {
                accent = cell.palette.color(QPalette::Mid);
            } else {
                accent = cell.palette.color(QPalette::Mid);
            }

            QColor foreground = accent;
            if (stockKind == QLatin1String("normal")
                || stockKind == QLatin1String("service")) {
                const QVariant foregroundData = index.data(Qt::ForegroundRole);
                foreground = foregroundData.isValid()
                    ? foregroundData.value<QBrush>().color()
                    : cell.palette.color(QPalette::Text);
            }
            QColor fill = accent;
            fill.setAlpha(24);
            QColor outline = accent;
            outline.setAlpha(190);

            painter->setPen(QPen(outline, 1));
            painter->setBrush(fill);
            painter->drawRoundedRect(chipRect, 12, 12);
            painter->setPen(foreground);
            QFont chipFont = cell.font;
            chipFont.setBold(true);
            painter->setFont(chipFont);
            painter->drawText(chipRect, Qt::AlignCenter, text);
        }
        painter->restore();
    }

    // Double-clicking a stock cell edits the quantity in a compact number box
    // shaped like the stock chip. Only plain stock (not services/variants).
    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                          const QModelIndex& index) const override
    {
        if (index.column() != InventoryConstants::COL_PRODUCT_STOCK)
            return nullptr;
        const QString kind = index.data(StockKindRole).toString();
        if (kind != QLatin1String("normal") && kind != QLatin1String("low") && kind != QLatin1String("out"))
            return nullptr;
        Q_UNUSED(option)
        auto* editor = new QSpinBox(parent);
        editor->setObjectName("stockEditor");
        editor->setButtonSymbols(QAbstractSpinBox::NoButtons);
        editor->setRange(0, 9999999);
        editor->setAlignment(Qt::AlignCenter);
        editor->setFrame(false);
        editor->setAccelerated(false);
        const bool light = StyleManager::loadColorScheme() == StyleManager::ColorScheme::Light;
        editor->setStyleSheet(QString(
            "QSpinBox#stockEditor { background-color: %1; color: %2; border: 1.5px solid %3; border-radius: 12px;"
            " padding: 0px 8px; min-height: 0px; font-weight: bold; selection-background-color: %3; selection-color: white; }")
            .arg(light ? "#FFFFFF" : "#1B2332", light ? "#40331F" : "#E6EAF0", light ? "#C8894A" : "#4A90E2"));
        return editor;
    }

    void setEditorData(QWidget* editor, const QModelIndex& index) const override
    {
        if (auto* spin = qobject_cast<QSpinBox*>(editor)) {
            spin->setValue(index.data(StockQuantityRole).toInt());
            // Select the number so typing replaces it.
            QTimer::singleShot(0, spin, [spin] { spin->selectAll(); });
            return;
        }
        QStyledItemDelegate::setEditorData(editor, index);
    }

    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override
    {
        if (auto* spin = qobject_cast<QSpinBox*>(editor)) {
            spin->interpretText();
            model->setData(index, QString::number(spin->value()), Qt::EditRole);
            return;
        }
        QStyledItemDelegate::setModelData(editor, model, index);
    }

    void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option,
                              const QModelIndex&) const override
    {
        const QRect bounds = option.rect.adjusted(4, 4, -4, -4);
        const int w = qMin(bounds.width(), 110);
        const int h = qMin(bounds.height(), 32);
        editor->setGeometry(bounds.center().x() - w / 2 + 1, bounds.center().y() - h / 2 + 1, w, h);
    }

private:
    static QString stockDisplayText(const QString& kind, int quantity)
    {
        if (kind == QLatin1String("out"))
            return QCoreApplication::translate("InventoryWindow", "Out of stock");
        if (kind == QLatin1String("low"))
            return QCoreApplication::translate("InventoryWindow", "%1 left").arg(quantity);
        if (kind == QLatin1String("variants"))
            return QCoreApplication::translate("InventoryWindow", "Variants");
        if (kind == QLatin1String("service"))
            return QCoreApplication::translate("InventoryWindow", "Service");
        return QString::number(quantity);
    }
};

} // namespace

InventoryWindow::InventoryWindow(UserSession* session, LocalApi* api,
                                 LocalImages* images, ChangeNotifier* notifier,
                                 QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_session(session)
    , m_api(api)
    , m_images(images)
    , m_notifier(notifier)
    , m_searchTimer(new QTimer(this))
    , m_currentSearch("")
    , m_flashTimer(nullptr)
    , m_flashState(true)
    , m_currentTable(InventoryConstants::TABLE_PRODUCTS)
    , m_stockFilter(StockFilter::All)
    , m_stockFilterLowThreshold(InventoryConstants::LOW_STOCK_THRESHOLD)
    , m_isEditingStock(false)
    , m_editingStockRow(-1)
    , m_blockCellChanged(false)
{
    ui->setupUi(this);
    InventoryUIMap::initialize(ui);

    // Setup notification system
    m_bell = new NotificationBell(m_api, InventoryUIMap::notificationBtn,
                                  m_session ? m_session->username() : QString(),
                                  m_notifier, this);



    setupUI();
    setupConnections();
    setupTableHeaders(m_currentTable);
    loadCurrentTable();
    showMaximized();
}

InventoryWindow::~InventoryWindow()
{
    // m_flashTimer is parented to this window, so Qt cleans it up automatically.
    // Setting to nullptr here is just for clarity.
    m_flashTimer = nullptr;
    delete ui;
}

void InventoryWindow::setupUI()
{
    setWindowTitle(tr("POSBox - Inventory"));

    m_searchTimer->setSingleShot(true);
    m_searchTimer->setInterval(AppConstants::SEARCH_DELAY_MS);

    QWidget* central = centralWidget();
    if (central) {
        central->setObjectName("centralwidget");
    }

    // Permission-based visibility for Delete
    if (m_session && InventoryUIMap::deleteBtn) {
        bool canDelete = Permissions::can(
            m_api, m_session->username(), m_session->role(),
            AccessControl::PERMISSION_DELETE_PRODUCT);
        InventoryUIMap::deleteBtn->setVisible(canDelete);
    }

    // ── Build Proposal A layout programmatically ─────────────────────────
    QVBoxLayout* rootLayout = qobject_cast<QVBoxLayout*>(central->layout());
    if (!rootLayout) {
        rootLayout = new QVBoxLayout(central);
    } else {
        while (QLayoutItem* item = rootLayout->takeAt(0))
            delete item;
    }
    rootLayout->setContentsMargins(40, 40, 40, 40);
    rootLayout->setSpacing(16);

    // Create footer label (not in .ui file)
    InventoryUIMap::footerLabel = new QLabel();
    InventoryUIMap::footerLabel->setObjectName("footerLabel");
    InventoryUIMap::footerLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    // 1) HEADER: title/subtitle left, KPI strip right
    setupHeader(rootLayout);

    // 2) TABS, SEARCH, AND ACTIONS share the reference's toolbar row.
    setupActionBar(rootLayout);

    // 3) MAIN TABLE
    rootLayout->addWidget(InventoryUIMap::mainTable, 1);

    // 4) FOOTER: sorting/info line
    rootLayout->addWidget(InventoryUIMap::footerLabel);
    // Table configuration.
    QTableWidget* t = InventoryUIMap::mainTable;
    QHeaderView*  hh = t->horizontalHeader();
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setSelectionMode(QAbstractItemView::SingleSelection);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->verticalHeader()->setVisible(false);
    t->setAlternatingRowColors(true);
    t->setShowGrid(true);
    t->setObjectName("mainTable");
    auto* delegate = new InventoryCellDelegate(t);
    t->setItemDelegate(delegate);
    connect(delegate, &QAbstractItemDelegate::closeEditor, this, [this]() {
        m_isEditingStock = false;
        QTimer::singleShot(0, this, [this]() {
            if (!m_reloadPending) return;
            m_reloadPending = false;
            loadCurrentTable();
        });
    });
    hh->setSectionsMovable(false);
    hh->setSectionsClickable(false);
    t->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    t->verticalHeader()->setDefaultSectionSize(64);
}

// ── Header: Inventory title + KPI strip ─────────────────────────────────

void InventoryWindow::setupHeader(QVBoxLayout* rootLayout)
{
    QWidget* headerWidget = new QWidget();
    headerWidget->setObjectName("headerWidget");
    QHBoxLayout* headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(16);

    // Title + subtitle on the left
    QWidget* titleBlock = new QWidget();
    QVBoxLayout* titleLayout = new QVBoxLayout(titleBlock);
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->setSpacing(2);

    InventoryUIMap::headerTitle = new QLabel(tr("Inventory"));
    InventoryUIMap::headerTitle->setObjectName("headerTitle");

    InventoryUIMap::headerSubtitle = new QLabel(tr("Products, pricing and stock"));
    InventoryUIMap::headerSubtitle->setObjectName("headerSubtitle");

    titleLayout->addWidget(InventoryUIMap::headerTitle);
    titleLayout->addWidget(InventoryUIMap::headerSubtitle);

    headerLayout->addWidget(titleBlock);

    // Spacer pushes KPI strip to the right
    headerLayout->addStretch(1);

    // KPI strip: 3 pill labels
    QWidget* kpiStrip = new QWidget();
    kpiStrip->setObjectName("kpiStrip");
    QHBoxLayout* kpiLayout = new QHBoxLayout(kpiStrip);
    kpiLayout->setContentsMargins(0, 0, 0, 0);
    kpiLayout->setSpacing(8);

    auto makeKpiLabel = [&](QPushButton*& ptr, const QString& objectName) {
        ptr = new QPushButton("0");
        ptr->setObjectName(objectName);
        ptr->setFlat(true);
        ptr->setAutoDefault(false);
        ptr->setCursor(Qt::PointingHandCursor);
        kpiLayout->addWidget(ptr);
    };

    makeKpiLabel(InventoryUIMap::productsCountLabel, "kpiLabel");
    makeKpiLabel(InventoryUIMap::outOfStockLabel,    "kpiLabelOut");
    makeKpiLabel(InventoryUIMap::lowStockLabel,      "kpiLabelLow");
    updateStockFilterButtons();

    headerLayout->addWidget(kpiStrip);

    InventoryUIMap::logoutBtn->setObjectName("inventoryLogoutBtn");
    InventoryUIMap::logoutBtn->setText(QString());
    InventoryUIMap::notificationBtn->setObjectName("notificationBtn");
    InventoryUIMap::notificationBtn->setFixedSize(36, 36);
    InventoryUIMap::logoutBtn->setFixedSize(36, 36);
    headerLayout->addWidget(InventoryUIMap::notificationBtn);
    headerLayout->addWidget(InventoryUIMap::logoutBtn);

    rootLayout->addWidget(headerWidget);
}

// ── Tab bar: Products / Categories / Discounts pill buttons ─────────────

void InventoryWindow::setupTabButtons(QHBoxLayout* actionLayout)
{
    auto* tabWidget = new InventoryTabBar();
    m_tabBar = tabWidget;
    tabWidget->setObjectName("tabBarWidget");
    QHBoxLayout* tabLayout = new QHBoxLayout(tabWidget);
    tabLayout->setContentsMargins(4, 4, 4, 4);
    tabLayout->setSpacing(2);

    auto makeTab = [&](QPushButton*& ptr, const QString& text, const QString& objName, bool active) {
        ptr = new InventoryTabButton(text);
        ptr->setObjectName(objName);
        ptr->setCheckable(false);
        ptr->setProperty("tabActive", active ? "true" : "false");
        ptr->setCursor(Qt::PointingHandCursor);
        ptr->setProperty("posboxNoFeedback", true);
        tabLayout->addWidget(ptr);
        tabWidget->watchTab(ptr);
    };

    makeTab(InventoryUIMap::productsTab,   tr("Products"),   "tabProducts", true);
    makeTab(InventoryUIMap::categoriesTab, tr("Categories"), "tabCategories", false);
    makeTab(InventoryUIMap::discountsTab,  tr("Discounts"),  "tabDiscounts", false);

    actionLayout->addWidget(tabWidget);
    tabWidget->setActive(InventoryUIMap::productsTab);

    // Connect tab buttons
    connect(InventoryUIMap::productsTab,   &QPushButton::clicked, this, [this]() { onTabClicked(InventoryConstants::TABLE_PRODUCTS); });
    connect(InventoryUIMap::categoriesTab, &QPushButton::clicked, this, [this]() { onTabClicked(InventoryConstants::TABLE_CATEGORIES); });
    connect(InventoryUIMap::discountsTab,  &QPushButton::clicked, this, [this]() { onTabClicked(InventoryConstants::TABLE_DISCOUNTS); });
}

// ── Search + action bar ─────────────────────────────────────────────────

void InventoryWindow::setupActionBar(QVBoxLayout* rootLayout)
{
    QWidget* actionWidget = new QWidget();
    actionWidget->setObjectName("actionBarWidget");
    QHBoxLayout* actionLayout = new QHBoxLayout(actionWidget);
    actionLayout->setContentsMargins(0, 0, 0, 0);
    actionLayout->setSpacing(10);

    setupTabButtons(actionLayout);

    // Search box (left)
    QWidget* searchBlock = new QWidget();
    QHBoxLayout* searchLayout = new QHBoxLayout(searchBlock);
    searchLayout->setContentsMargins(0, 0, 0, 0);
    searchLayout->setSpacing(8);

    InventoryUIMap::searchBox->setObjectName("searchBox");
    InventoryUIMap::searchBox->setPlaceholderText(tr("Search name, category, barcode..."));
    searchLayout->addWidget(InventoryUIMap::searchBox);

    actionLayout->addWidget(searchBlock, 1);

    actionLayout->addSpacing(8);

    // Action buttons: Add (green), Edit (blue), Delete (red)
    InventoryUIMap::addBtn->setObjectName("actionAdd");
    InventoryUIMap::addBtn->setText(tr("Add"));
    InventoryUIMap::addBtn->setProperty("btnStyle", "add");
    InventoryUIMap::addBtn->setCursor(Qt::PointingHandCursor);
    actionLayout->addWidget(InventoryUIMap::addBtn);

    InventoryUIMap::editBtn->setObjectName("actionEdit");
    InventoryUIMap::editBtn->setText(tr("Edit"));
    InventoryUIMap::editBtn->setProperty("btnStyle", "edit");
    InventoryUIMap::editBtn->setCursor(Qt::PointingHandCursor);
    actionLayout->addWidget(InventoryUIMap::editBtn);

    InventoryUIMap::deleteBtn->setObjectName("actionDelete");
    InventoryUIMap::deleteBtn->setText(tr("Delete"));
    InventoryUIMap::deleteBtn->setProperty("btnStyle", "delete");
    InventoryUIMap::deleteBtn->setCursor(Qt::PointingHandCursor);
    actionLayout->addWidget(InventoryUIMap::deleteBtn);

    rootLayout->addWidget(actionWidget);
}

// ── Tab click handler (replaces QComboBox tableSelect) ──────────────────

void InventoryWindow::onTabClicked(int index)
{
    m_stockFilter = StockFilter::All;
    m_stockFilterLowThreshold = InventoryConstants::LOW_STOCK_THRESHOLD;
    updateStockFilterButtons();

    // Update tab button states
    InventoryUIMap::productsTab->setProperty("tabActive", index == InventoryConstants::TABLE_PRODUCTS ? "true" : "false");
    InventoryUIMap::productsTab->style()->unpolish(InventoryUIMap::productsTab);
    InventoryUIMap::productsTab->style()->polish(InventoryUIMap::productsTab);
    InventoryUIMap::productsTab->update();

    InventoryUIMap::categoriesTab->setProperty("tabActive", index == InventoryConstants::TABLE_CATEGORIES ? "true" : "false");
    InventoryUIMap::categoriesTab->style()->unpolish(InventoryUIMap::categoriesTab);
    InventoryUIMap::categoriesTab->style()->polish(InventoryUIMap::categoriesTab);
    InventoryUIMap::categoriesTab->update();

    InventoryUIMap::discountsTab->setProperty("tabActive", index == InventoryConstants::TABLE_DISCOUNTS ? "true" : "false");
    InventoryUIMap::discountsTab->style()->unpolish(InventoryUIMap::discountsTab);
    InventoryUIMap::discountsTab->style()->polish(InventoryUIMap::discountsTab);
    InventoryUIMap::discountsTab->update();

    static_cast<InventoryTabBar*>(m_tabBar)->setActive(
        index == InventoryConstants::TABLE_PRODUCTS ? InventoryUIMap::productsTab
        : index == InventoryConstants::TABLE_CATEGORIES ? InventoryUIMap::categoriesTab
                                                        : InventoryUIMap::discountsTab);

    Animations::fadeChange(InventoryUIMap::mainTable);
    m_currentTable = index;
    InventoryUIMap::searchBox->clear();
    m_currentSearch.clear();
    setupTableHeaders(index);
    loadCurrentTable();

    if (InventoryUIMap::editBtn)   InventoryUIMap::editBtn->setEnabled(false);
    if (InventoryUIMap::deleteBtn) InventoryUIMap::deleteBtn->setEnabled(false);

    // Stop flashing when leaving the products table
    if (index != InventoryConstants::TABLE_PRODUCTS) {
        if (m_flashTimer) {
            m_flashTimer->stop();
            m_flashTimer->deleteLater();
            m_flashTimer = nullptr;
        }
    }
}

// ── KPI strip update ────────────────────────────────────────────────────

void InventoryWindow::updateKPIs(int totalProducts, int outOfStock, int lowStock)
{
    const auto show = [](QPushButton* label, int value, const QString& text) {
        if (!label)
            return;
        Animations::animateValues(label, {double(value)}, [label, text](const QVector<double>& shown) {
            label->setText(QString("%1 %2").arg(qRound(shown[0])).arg(text));
        });
    };
    show(InventoryUIMap::productsCountLabel, totalProducts, tr("products"));
    show(InventoryUIMap::outOfStockLabel, outOfStock, tr("out of stock"));
    show(InventoryUIMap::lowStockLabel, lowStock, tr("low stock"));
}

void InventoryWindow::setStockFilter(StockFilter filter, int lowStockThreshold)
{
    m_stockFilter = filter;
    m_stockFilterLowThreshold = qMax(1, lowStockThreshold);
    updateStockFilterButtons();
    applyCurrentStockFilter();
}

void InventoryWindow::updateStockFilterButtons()
{
    const QList<QPair<QPushButton*, StockFilter>> buttons = {
        {InventoryUIMap::productsCountLabel, StockFilter::All},
        {InventoryUIMap::outOfStockLabel, StockFilter::OutOfStock},
        {InventoryUIMap::lowStockLabel, StockFilter::LowStock}
    };
    for (const auto& entry : buttons) {
        QPushButton* button = entry.first;
        if (!button)
            continue;
        button->setProperty("filterActive", entry.second == m_stockFilter);
        button->style()->unpolish(button);
        button->style()->polish(button);
        button->update();
    }
}

void InventoryWindow::applyCurrentStockFilter()
{
    QTableWidget* table = InventoryUIMap::mainTable;
    if (!table || m_currentTable != InventoryConstants::TABLE_PRODUCTS)
        return;

    for (int row = 0; row < table->rowCount(); ++row) {
        QTableWidgetItem* stockItem =
            table->item(row, InventoryConstants::COL_PRODUCT_STOCK);
        bool ok = false;
        const int stock = stockItem ? stockItem->text().toInt(&ok) : 0;
        bool visible = true;
        if (m_stockFilter == StockFilter::OutOfStock)
            visible = ok && stock == InventoryConstants::OUT_OF_STOCK_VALUE;
        else if (m_stockFilter == StockFilter::LowStock)
            visible = ok && stock > 0 && stock <= m_stockFilterLowThreshold;
        table->setRowHidden(row, !visible);
    }
}

void InventoryWindow::setupConnections()
{
    if (m_images) {
        connect(m_images, &LocalImages::pixmapReady,
                this, &InventoryWindow::onProductImageReady);
    }

    connect(InventoryUIMap::searchBox, &QLineEdit::textChanged,
            this, &InventoryWindow::onSearchChanged);
    connect(m_searchTimer, &QTimer::timeout,
            this, &InventoryWindow::onSearchTimer);
    connect(InventoryUIMap::addBtn, &QPushButton::clicked,
            this, &InventoryWindow::onAddClicked);
    connect(InventoryUIMap::editBtn, &QPushButton::clicked,
            this, &InventoryWindow::onEditClicked);
    connect(InventoryUIMap::deleteBtn, &QPushButton::clicked,
            this, &InventoryWindow::onDeleteClicked);
    connect(InventoryUIMap::productsCountLabel, &QPushButton::clicked, this, [this]() {
        if (m_currentTable != InventoryConstants::TABLE_PRODUCTS)
            onTabClicked(InventoryConstants::TABLE_PRODUCTS);
        setStockFilter(StockFilter::All, InventoryConstants::LOW_STOCK_THRESHOLD);
    });
    connect(InventoryUIMap::outOfStockLabel, &QPushButton::clicked, this, [this]() {
        if (m_currentTable != InventoryConstants::TABLE_PRODUCTS)
            onTabClicked(InventoryConstants::TABLE_PRODUCTS);
        setStockFilter(StockFilter::OutOfStock, InventoryConstants::LOW_STOCK_THRESHOLD);
    });
    connect(InventoryUIMap::lowStockLabel, &QPushButton::clicked, this, [this]() {
        if (m_currentTable != InventoryConstants::TABLE_PRODUCTS)
            onTabClicked(InventoryConstants::TABLE_PRODUCTS);
        setStockFilter(StockFilter::LowStock, InventoryConstants::LOW_STOCK_THRESHOLD);
    });
    connect(InventoryUIMap::notificationBtn, &QPushButton::clicked, m_bell, &NotificationBell::togglePanel);
    connect(InventoryUIMap::logoutBtn, &QPushButton::clicked,
            this, &InventoryWindow::onLogoutClicked);
    connect(InventoryUIMap::mainTable, &QTableWidget::itemSelectionChanged,
            this, &InventoryWindow::onSelectionChanged);
    connect(InventoryUIMap::mainTable, &QTableWidget::cellDoubleClicked,
            this, &InventoryWindow::onCellDoubleClicked);
    connect(InventoryUIMap::mainTable, &QTableWidget::cellChanged,
            this, &InventoryWindow::onCellChanged);

    if (m_notifier) {
        connect(m_notifier, &ChangeNotifier::productChanged,
                this, [this](int) { if (m_currentTable == InventoryConstants::TABLE_PRODUCTS) loadCurrentTable(); m_bell->refreshNow(); });
        connect(m_notifier, &ChangeNotifier::categoryChanged,
                this, [this](int) { if (m_currentTable == InventoryConstants::TABLE_CATEGORIES) loadCurrentTable(); });
        connect(m_notifier, &ChangeNotifier::discountChanged,
                this, [this](int) { if (m_currentTable == InventoryConstants::TABLE_DISCOUNTS) loadCurrentTable(); });
        connect(m_notifier, &ChangeNotifier::appNavigateRequested,
                this, &InventoryWindow::handleNavigation);
    }
}

void InventoryWindow::setupTableHeaders(int tableIndex)
{
    if (!InventoryUIMap::mainTable) return;

    QTableWidget* t = InventoryUIMap::mainTable;
    QHeaderView*  h = t->horizontalHeader();

    t->clearContents();
    t->setRowCount(0);

    h->setSectionsMovable(false);
    h->setSectionsClickable(false);

    switch (tableIndex) {

    case InventoryConstants::TABLE_PRODUCTS:
        t->setColumnCount(InventoryConstants::PRODUCT_COL_COUNT);
        t->setHorizontalHeaderLabels({ tr("ID"), "", tr("Name"), tr("Category"), tr("Stock"), tr("Cost"), tr("Price"), tr("Margin")});
        h->setSectionResizeMode(InventoryConstants::COL_PRODUCT_ID,       QHeaderView::Fixed);
        h->setSectionResizeMode(InventoryConstants::COL_PRODUCT_BADGE,    QHeaderView::Fixed);
        h->setSectionResizeMode(InventoryConstants::COL_PRODUCT_NAME,     QHeaderView::Fixed);
        h->setSectionResizeMode(InventoryConstants::COL_PRODUCT_CATEGORY, QHeaderView::Fixed);
        h->setSectionResizeMode(InventoryConstants::COL_PRODUCT_STOCK,    QHeaderView::Fixed);
        h->setSectionResizeMode(InventoryConstants::COL_PRODUCT_COST,     QHeaderView::Fixed);
        h->setSectionResizeMode(InventoryConstants::COL_PRODUCT_PRICE,    QHeaderView::Fixed);
        h->setSectionResizeMode(InventoryConstants::COL_PRODUCT_MARGIN,   QHeaderView::Fixed);
        // ID column hidden — used internally only
        t->setColumnWidth(InventoryConstants::COL_PRODUCT_ID,       0);
        applyProductColumnRatios();
        t->setColumnHidden(InventoryConstants::COL_PRODUCT_ID, true);
        break;

    case InventoryConstants::TABLE_CATEGORIES:
        t->setColumnCount(InventoryConstants::CATEGORY_COL_COUNT);
        t->setHorizontalHeaderLabels({ tr("Name"), tr("Description")});
        t->setColumnHidden(InventoryConstants::COL_CATEGORY_NAME, false);
        t->setColumnHidden(InventoryConstants::COL_CATEGORY_DESCRIPTION, false);
        h->setSectionResizeMode(InventoryConstants::COL_CATEGORY_NAME,        QHeaderView::Fixed);
        h->setSectionResizeMode(InventoryConstants::COL_CATEGORY_DESCRIPTION, QHeaderView::Stretch);
        applyCategoryColumnRatio();
        break;

    case InventoryConstants::TABLE_DISCOUNTS:
        t->setColumnCount(InventoryConstants::DISCOUNT_COL_COUNT);
        t->setHorizontalHeaderLabels({ tr("Name"), tr("Value"), tr("Active"), tr("End Date")});
        for (int column = 0; column < InventoryConstants::DISCOUNT_COL_COUNT; ++column)
            t->setColumnHidden(column, false);
        h->setSectionResizeMode(InventoryConstants::COL_DISCOUNT_NAME,     QHeaderView::Stretch);
        h->setSectionResizeMode(InventoryConstants::COL_DISCOUNT_VALUE,    QHeaderView::Stretch);
        h->setSectionResizeMode(InventoryConstants::COL_DISCOUNT_ACTIVE,   QHeaderView::Stretch);
        h->setSectionResizeMode(InventoryConstants::COL_DISCOUNT_END_DATE, QHeaderView::Stretch);
        break;
    }
}

void InventoryWindow::applyProductColumnRatios()
{
    if (!InventoryUIMap::mainTable) return;
    if (m_currentTable != InventoryConstants::TABLE_PRODUCTS) return;

    const int totalWidth = InventoryUIMap::mainTable->viewport()->width();
    if (totalWidth <= 0) return;

    QTableWidget* table = InventoryUIMap::mainTable;
    table->setColumnWidth(InventoryConstants::COL_PRODUCT_BADGE,
                          qRound(totalWidth * 0.052));
    table->setColumnWidth(InventoryConstants::COL_PRODUCT_CATEGORY,
                          qRound(totalWidth * 0.14));
    table->setColumnWidth(InventoryConstants::COL_PRODUCT_STOCK,
                          qRound(totalWidth * 0.16));
    table->setColumnWidth(InventoryConstants::COL_PRODUCT_COST,
                          qRound(totalWidth * 0.112));
    table->setColumnWidth(InventoryConstants::COL_PRODUCT_PRICE,
                          qRound(totalWidth * 0.112));
    table->setColumnWidth(InventoryConstants::COL_PRODUCT_MARGIN,
                          qRound(totalWidth * 0.103));

    const int nameWidth = totalWidth
        - table->columnWidth(InventoryConstants::COL_PRODUCT_BADGE)
        - table->columnWidth(InventoryConstants::COL_PRODUCT_CATEGORY)
        - table->columnWidth(InventoryConstants::COL_PRODUCT_STOCK)
        - table->columnWidth(InventoryConstants::COL_PRODUCT_COST)
        - table->columnWidth(InventoryConstants::COL_PRODUCT_PRICE)
        - table->columnWidth(InventoryConstants::COL_PRODUCT_MARGIN);
    table->setColumnWidth(InventoryConstants::COL_PRODUCT_NAME, qMax(0, nameWidth));
}

void InventoryWindow::applyCategoryColumnRatio()
{
    if (!InventoryUIMap::mainTable) return;
    if (m_currentTable != InventoryConstants::TABLE_CATEGORIES) return;

    int totalWidth = InventoryUIMap::mainTable->viewport()->width();
    if (totalWidth <= 0) return;

    int nameWidth = totalWidth / 3;
    InventoryUIMap::mainTable->setColumnWidth(InventoryConstants::COL_CATEGORY_NAME, nameWidth);
}

void InventoryWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    applyProductColumnRatios();
    applyCategoryColumnRatio();
}

// ===== TABLE SWITCHING & SEARCH =====

/// onTableSelectChanged replaced by onTabClicked (see setupTabButtons)

void InventoryWindow::onSearchChanged(const QString& text)
{
    m_currentSearch = text;
    m_searchTimer->start();
}

void InventoryWindow::onSearchTimer()
{
    loadCurrentTable();
}

void InventoryWindow::loadCurrentTable()
{
    // Never rebuild the table under the stock editor: refresh once it closes.
    if (m_isEditingStock) {
        m_reloadPending = true;
        return;
    }
    switch (m_currentTable) {
    case InventoryConstants::TABLE_PRODUCTS:   loadProducts(m_currentSearch);   break;
    case InventoryConstants::TABLE_CATEGORIES: loadCategories(m_currentSearch); break;
    case InventoryConstants::TABLE_DISCOUNTS:  loadDiscounts(m_currentSearch);  break;
    }
}

// ===== DATA LOADING =====

void InventoryWindow::loadProducts(const QString& search)
{
    if (!m_api || !InventoryUIMap::mainTable) return;

    m_blockCellChanged = true;
    InventoryUIMap::mainTable->setRowCount(0);

    // product_list includes the computed stock;
    // ordering is by name already, which the bucket sort below relies on.
    QList<DataRow> rows = m_api->products();
    if (!m_api->ok()) {
        qWarning() << "Failed to load products:" << m_api->lastError();
        m_blockCellChanged = false;
        return;
    }

    // ── Collect all rows into buckets for sorting ────────────────────────────
    //
    // Sort order:
    //   Bucket 0 — out of stock  (tracked, non-service, non-variant, stock == 0)
    //   Bucket 1 — low stock     (tracked, non-service, non-variant, 1 <= stock <= LOW_STOCK_THRESHOLD)
    //   Bucket 2 — normal / no-stock-tracking items (services, variants, stock > LOW_STOCK_THRESHOLD)
    //
    // Within each bucket rows are kept in the ORDER BY p.name order (already
    // alphabetical). Services and variant products land in bucket 2 so they're
    // interspersed alphabetically among normal-stock items.
    //
    // Search filtering is done here (the generic select is
    // equality-only): name OR category name contains the term.

    struct ProductRow {
        int     id;
        QString name;
        QString imagePath;
        int     stock;
        double  cost;
        double  price;
        bool    isService;
        bool    hasVariants;
        int     bucket; // 0 = out-of-stock, 1 = low, 2 = normal/no-stock
        QString categoryName; // NEW for Proposal A
    };

    QList<ProductRow> bucket0; // out of stock
    QList<ProductRow> bucket1; // low stock
    QList<ProductRow> bucket2; // normal / service / variant

    int totalProducts = 0;
    int outOfStockCount = 0;
    int lowStockCount = 0;

    for (const DataRow& row : rows) {
        const QString name = row.value("name").toString();
        const QString categoryName = row.value("category_name").toString();
        const QString barcode = row.value("barcode").toString();

        if (!search.isEmpty()
            && !name.contains(search, Qt::CaseInsensitive)
            && !categoryName.contains(search, Qt::CaseInsensitive)
            && !barcode.contains(search, Qt::CaseInsensitive))
            continue;

        ProductRow r;
        r.id          = row.value("id").toInt();
        r.name        = name;
        r.imagePath   = row.value("image_path").toString();
        r.stock       = row.value("stock_quantity").toInt();
        r.cost        = row.value("cost").toDouble();
        r.price       = row.value("price").toDouble();
        r.isService   = row.value("is_service").toInt() != 0;
        r.hasVariants = row.value("has_variants").toInt() != 0;
        r.categoryName = categoryName;

        totalProducts++;

        // Only tracked, non-service, non-variant products participate in stock sorting
        bool stockTracked = !r.isService && !r.hasVariants;

        if (stockTracked && r.stock == InventoryConstants::OUT_OF_STOCK_VALUE) {
            r.bucket = 0;
            outOfStockCount++;
            bucket0.append(r);
        } else if (stockTracked && r.stock <= InventoryConstants::LOW_STOCK_THRESHOLD) {
            r.bucket = 1;
            lowStockCount++;
            bucket1.append(r);
        } else {
            r.bucket = 2;
            bucket2.append(r);
        }
    }

    // Update KPI strip
    updateKPIs(totalProducts, outOfStockCount, lowStockCount);

    // Footer sorting info (matches Proposal A footer)
    if (InventoryUIMap::footerLabel) {
        InventoryUIMap::footerLabel->setText(
            tr("%1 products · out-of-stock first · low stock second · name order within each group")
            .arg(totalProducts));
    }

    // ── Insert rows in sorted order: bucket0, bucket1, bucket2 ───────────────
    // Each bucket is already alphabetically sorted because the query orders by name.

    auto insertRows = [&](const QList<ProductRow>& rows) {
        for (const ProductRow& r : rows) {
            int tableRow = InventoryUIMap::mainTable->rowCount();
            InventoryUIMap::mainTable->insertRow(tableRow);

            // ID column (hidden)
            QTableWidgetItem* idItem = new QTableWidgetItem(QString::number(r.id));
            idItem->setTextAlignment(Qt::AlignCenter);
            InventoryUIMap::mainTable->setItem(tableRow, InventoryConstants::COL_PRODUCT_ID, idItem);

            // Separate badge and name cells to match the reference table.
            QString letter = r.name.left(1).toUpper();
            QTableWidgetItem* badgeItem = new QTableWidgetItem(letter);
            badgeItem->setTextAlignment(Qt::AlignCenter);
            badgeItem->setData(BadgeColorRole, badgeColorForLetter(letter));
            badgeItem->setData(ProductImagePathRole, r.imagePath);
            if (!r.imagePath.isEmpty() && m_images) {
                if (m_images->isCached(r.imagePath, ProductImageSize)) {
                    badgeItem->setData(
                        ProductImagePixmapRole,
                        m_images->thumbnail(r.imagePath, ProductImageSize));
                } else {
                    m_images->requestPixmap(r.imagePath, ProductImageSize);
                }
            }
            InventoryUIMap::mainTable->setItem(
                tableRow, InventoryConstants::COL_PRODUCT_BADGE, badgeItem);

            QTableWidgetItem* nameItem = new QTableWidgetItem(r.name);
            nameItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            InventoryUIMap::mainTable->setItem(tableRow, InventoryConstants::COL_PRODUCT_NAME, nameItem);

            // Category column
            QTableWidgetItem* categoryItem = new QTableWidgetItem(r.categoryName);
            categoryItem->setTextAlignment(Qt::AlignCenter);
            InventoryUIMap::mainTable->setItem(tableRow, InventoryConstants::COL_PRODUCT_CATEGORY, categoryItem);

            // The item keeps a raw numeric value for filtering and inline stock
            // editing; the delegate renders the user-facing stock chip.
            QString stockText;
            QString stockKind;
            if (r.isService) {
                stockText = tr("Service");
                stockKind = "service";
            } else if (r.hasVariants) {
                stockText = tr("Variants");
                stockKind = "variants";
            } else {
                stockText = QString::number(r.stock);
                stockKind = r.stock == InventoryConstants::OUT_OF_STOCK_VALUE
                    ? "out"
                    : (r.stock <= InventoryConstants::LOW_STOCK_THRESHOLD
                           ? "low" : "normal");
            }
            QTableWidgetItem* stockItem = new QTableWidgetItem(stockText);
            stockItem->setTextAlignment(Qt::AlignCenter);
            stockItem->setData(StockKindRole, stockKind);
            stockItem->setData(StockQuantityRole, r.stock);
            if (stockKind == QLatin1String("service") || stockKind == QLatin1String("variants"))
                stockItem->setFlags(stockItem->flags() & ~Qt::ItemIsEditable);
            InventoryUIMap::mainTable->setItem(tableRow, InventoryConstants::COL_PRODUCT_STOCK, stockItem);

            // Cost / price: empty for variant products
            QTableWidgetItem* costItem = new QTableWidgetItem(
                r.hasVariants ? "" : AppHelpers::formatPrice(r.cost));
            costItem->setTextAlignment(Qt::AlignCenter);
            InventoryUIMap::mainTable->setItem(tableRow, InventoryConstants::COL_PRODUCT_COST, costItem);

            QTableWidgetItem* priceItem = new QTableWidgetItem(
                r.hasVariants ? "" : AppHelpers::formatPrice(r.price));
            priceItem->setTextAlignment(Qt::AlignCenter);
            InventoryUIMap::mainTable->setItem(tableRow, InventoryConstants::COL_PRODUCT_PRICE, priceItem);

            // Margin column: (price - cost) / price * 100, or — for variants/services
            QTableWidgetItem* marginItem;
            if (r.hasVariants || r.isService) {
                marginItem = new QTableWidgetItem("—");
            } else {
                double margin = 0.0;
                if (r.price > 0) {
                    margin = (r.price - r.cost) / r.price * 100.0;
                }
                marginItem = new QTableWidgetItem(QString("%1%").arg(margin, 0, 'f', 1));
            }
            marginItem->setTextAlignment(Qt::AlignCenter);
            InventoryUIMap::mainTable->setItem(tableRow, InventoryConstants::COL_PRODUCT_MARGIN, marginItem);
        }
    };

    insertRows(bucket0);
    insertRows(bucket1);
    insertRows(bucket2);

    m_blockCellChanged = false;
    applyCurrentStockFilter();

    qDebug() << "Loaded" << InventoryUIMap::mainTable->rowCount()
             << "products (out-of-stock:" << bucket0.size()
             << "low:" << bucket1.size()
             << "normal:" << bucket2.size() << ")";

    // Apply text colors and manage the flash timer
    applyStockColors();
    updateFlashTimer();
}

void InventoryWindow::loadCategories(const QString& search)
{
    if (!m_api || !InventoryUIMap::mainTable) return;

    m_blockCellChanged = true;
    InventoryUIMap::mainTable->setRowCount(0);

    QList<DataRow> rows = m_api->select(
        "categories", {}, search.isEmpty() ? "display_order, name" : "name");
    if (!m_api->ok()) {
        qWarning() << "Failed to load categories:" << m_api->lastError();
        m_blockCellChanged = false;
        return;
    }

    for (const DataRow& c : rows) {
        const QString name = c.value("name").toString();
        if (!search.isEmpty() && !name.contains(search, Qt::CaseInsensitive))
            continue;

        int row = InventoryUIMap::mainTable->rowCount();
        InventoryUIMap::mainTable->insertRow(row);

        QTableWidgetItem* nameItem = new QTableWidgetItem(name);
        nameItem->setData(Qt::UserRole, c.value("id").toInt());
        nameItem->setTextAlignment(Qt::AlignCenter);
        InventoryUIMap::mainTable->setItem(row, InventoryConstants::COL_CATEGORY_NAME, nameItem);

        QTableWidgetItem* descItem = new QTableWidgetItem(c.value("description").toString());
        descItem->setTextAlignment(Qt::AlignCenter);
        InventoryUIMap::mainTable->setItem(row, InventoryConstants::COL_CATEGORY_DESCRIPTION, descItem);
    }

    m_blockCellChanged = false;
    qDebug() << "Loaded" << InventoryUIMap::mainTable->rowCount() << "categories";
}

void InventoryWindow::loadDiscounts(const QString& search)
{
    if (!m_api || !InventoryUIMap::mainTable) return;

    m_blockCellChanged = true;
    InventoryUIMap::mainTable->setRowCount(0);

    QList<DataRow> rows = m_api->select("discounts", {}, "name");
    if (!m_api->ok()) {
        qWarning() << "Failed to load discounts:" << m_api->lastError();
        m_blockCellChanged = false;
        return;
    }

    for (const DataRow& d : rows) {
        const QString name = d.value("name").toString();
        if (!search.isEmpty() && !name.contains(search, Qt::CaseInsensitive))
            continue;

        int row = InventoryUIMap::mainTable->rowCount();
        InventoryUIMap::mainTable->insertRow(row);

        QString type    = d.value("type").toString();
        double  value   = d.value("value").toDouble();
        bool    isActive = d.value("is_active").toInt() != 0;
        QString endDate = d.value("end_date").toString();

        QString valueStr = (type == "percentage")
                               ? QString("%1%").arg(value, 0, 'f', 1)
                               : AppHelpers::formatPrice(value);

        QTableWidgetItem* nameItem = new QTableWidgetItem(name);
        nameItem->setData(Qt::UserRole, d.value("id").toInt());
        nameItem->setTextAlignment(Qt::AlignCenter);
        InventoryUIMap::mainTable->setItem(row, InventoryConstants::COL_DISCOUNT_NAME, nameItem);

        QTableWidgetItem* valueItem = new QTableWidgetItem(valueStr);
        valueItem->setTextAlignment(Qt::AlignCenter);
        InventoryUIMap::mainTable->setItem(row, InventoryConstants::COL_DISCOUNT_VALUE, valueItem);

        QTableWidgetItem* activeItem = new QTableWidgetItem(isActive ? tr("Yes") : "No");
        activeItem->setTextAlignment(Qt::AlignCenter);
        InventoryUIMap::mainTable->setItem(row, InventoryConstants::COL_DISCOUNT_ACTIVE, activeItem);

        QTableWidgetItem* endDateItem = new QTableWidgetItem(endDate);
        endDateItem->setTextAlignment(Qt::AlignCenter);
        InventoryUIMap::mainTable->setItem(row, InventoryConstants::COL_DISCOUNT_END_DATE, endDateItem);
    }

    m_blockCellChanged = false;
    qDebug() << "Loaded" << InventoryUIMap::mainTable->rowCount() << "discounts";
}

void InventoryWindow::onProductImageReady(const QString& filename, int maxSize,
                                          const QImage& image)
{
    if (image.isNull() || maxSize != ProductImageSize || !InventoryUIMap::mainTable)
        return;

    QTableWidget* table = InventoryUIMap::mainTable;
    for (int row = 0; row < table->rowCount(); ++row) {
        QTableWidgetItem* badge = table->item(row, InventoryConstants::COL_PRODUCT_BADGE);
        if (!badge)
            continue;
        if (QFileInfo(badge->data(ProductImagePathRole).toString()).fileName() != filename)
            continue;
        badge->setData(ProductImagePixmapRole, QPixmap::fromImage(image));
    }
    table->viewport()->update();
}

// ===== STOCK COLOR HELPERS =====

QString InventoryWindow::defaultTextColor() const
{
    // Match the theme currently applied so the flash "off" color is always correct
    return (StyleManager::loadColorScheme() == StyleManager::ColorScheme::Light)
               ? QString(InventoryConstants::COLOR_TEXT_LIGHT_THEME)
               : QString(InventoryConstants::COLOR_TEXT_DARK_THEME);
}

QColor InventoryWindow::badgeColorForLetter(const QString& letter) const
{
    // Deterministic color per letter — same letter always gets the same color.
    // Colors chosen to be legible on dark background.
    static const struct { const char* letter; const char* color; } map[] = {
        {"A", "#4A90E2"}, {"B", "#E74C3C"}, {"C", "#27AE60"}, {"D", "#F39C12"},
        {"E", "#9B59B6"}, {"F", "#1ABC9C"}, {"G", "#34495E"}, {"H", "#E67E22"},
        {"I", "#3498DB"}, {"J", "#9A5B7C"}, {"K", "#7FB3D5"}, {"L", "#D7BDE2"},
        {"M", "#F9E79F"}, {"N", "#AAB7B8"}, {"O", "#F5CBA7"}, {"P", "#A9CCE3"},
        {"Q", "#D2B4DE"}, {"R", "#AED6F1"}, {"S", "#A2D9CE"}, {"T", "#F5B7B1"},
        {"U", "#D5F5E3"}, {"V", "#FDEDEC"}, {"W", "#EAEDED"}, {"X", "#FEF9E7"},
        {"Y", "#FCF3CF"}, {"Z", "#D5DBDB"},
    };
    QString u = letter.toUpper();
    for (const auto& entry : map) {
        if (u == QString::fromLatin1(entry.letter))
            return QColor(QString::fromLatin1(entry.color));
    }
    return QColor("#9AA4B2"); // fallback gray
}

void InventoryWindow::applyStockColors()
{
    QTableWidget* t = InventoryUIMap::mainTable;
    if (!t) return;

    const QColor yellowColor(InventoryConstants::COLOR_LOW_STOCK);
    const QColor redColor(InventoryConstants::COLOR_OUT_OF_STOCK);
    const QColor defaultColor(defaultTextColor());

    const int rowCount = t->rowCount();
    for (int row = 0; row < rowCount; ++row) {
        QTableWidgetItem* stockItem = t->item(row, InventoryConstants::COL_PRODUCT_STOCK);
        if (!stockItem) continue;

        const QString stockText = stockItem->text().trimmed();

        // Empty stock cell means service or variant product — skip coloring entirely
        if (stockText.isEmpty()) continue;

        bool ok = false;
        const int stock = stockText.toInt(&ok);
        if (!ok) continue;

        if (stock == InventoryConstants::OUT_OF_STOCK_VALUE) {
            // Out of stock — apply red to name and stock cells.
            // The flash timer will toggle between red and default; start in "on" (red) state.
            const QColor color = m_flashState ? redColor : defaultColor;
            if (t->item(row, InventoryConstants::COL_PRODUCT_NAME))
                t->item(row, InventoryConstants::COL_PRODUCT_NAME)->setForeground(color);
            stockItem->setForeground(color);

        } else if (stock <= InventoryConstants::LOW_STOCK_THRESHOLD) {
            // Low stock — steady yellow on name and stock cells
            if (t->item(row, InventoryConstants::COL_PRODUCT_NAME))
                t->item(row, InventoryConstants::COL_PRODUCT_NAME)->setForeground(yellowColor);
            stockItem->setForeground(yellowColor);

        } else {
            // Normal stock — restore default text color
            const QColor normal(defaultTextColor());
            if (t->item(row, InventoryConstants::COL_PRODUCT_NAME))
                t->item(row, InventoryConstants::COL_PRODUCT_NAME)->setForeground(normal);
            stockItem->setForeground(normal);
        }
    }
}

void InventoryWindow::updateFlashTimer()
{
    if (m_currentTable != InventoryConstants::TABLE_PRODUCTS) return;

    QTableWidget* t = InventoryUIMap::mainTable;
    if (!t) return;

    // Check whether any out-of-stock rows exist
    bool hasOutOfStock = false;
    const int rowCount = t->rowCount();
    for (int row = 0; row < rowCount; ++row) {
        QTableWidgetItem* stockItem = t->item(row, InventoryConstants::COL_PRODUCT_STOCK);
        if (!stockItem) continue;
        const QString stockText = stockItem->text().trimmed();
        if (stockText.isEmpty()) continue; // service / variant — skip
        bool ok = false;
        if (stockText.toInt(&ok) == InventoryConstants::OUT_OF_STOCK_VALUE && ok) {
            hasOutOfStock = true;
            break;
        }
    }

    if (hasOutOfStock) {
        // Start timer only if not already running
        if (!m_flashTimer) {
            m_flashTimer = new QTimer(this);
            m_flashTimer->setInterval(InventoryConstants::STOCK_FLASH_INTERVAL_MS);
            connect(m_flashTimer, &QTimer::timeout, this, &InventoryWindow::onFlashTick);
            m_flashState = true; // start in "red visible" state
            m_flashTimer->start();
            qDebug() << "Flash timer started (out-of-stock items present)";
        }
    } else {
        // No out-of-stock rows — stop and destroy the timer to free resources
        if (m_flashTimer) {
            m_flashTimer->stop();
            m_flashTimer->deleteLater();
            m_flashTimer = nullptr;
            qDebug() << "Flash timer stopped (no out-of-stock items)";
        }
    }
}

void InventoryWindow::onFlashTick()
{
    // Toggle flash state and repaint only the out-of-stock rows
    m_flashState = !m_flashState;

    QTableWidget* t = InventoryUIMap::mainTable;
    if (!t) return;

    // Only update if we're still on the products table
    if (m_currentTable != InventoryConstants::TABLE_PRODUCTS) return;

    const QColor redColor(InventoryConstants::COLOR_OUT_OF_STOCK);
    const QColor defaultColor(defaultTextColor());
    const QColor activeColor = m_flashState ? redColor : defaultColor;
    const bool wasBlocked = m_blockCellChanged;
    m_blockCellChanged = true;

    const int rowCount = t->rowCount();
    for (int row = 0; row < rowCount; ++row) {
        QTableWidgetItem* stockItem = t->item(row, InventoryConstants::COL_PRODUCT_STOCK);
        if (!stockItem) continue;
        const QString stockText = stockItem->text().trimmed();
        if (stockText.isEmpty()) continue; // service / variant — skip

        bool ok = false;
        if (stockText.toInt(&ok) == InventoryConstants::OUT_OF_STOCK_VALUE && ok) {
            if (t->item(row, InventoryConstants::COL_PRODUCT_NAME))
                t->item(row, InventoryConstants::COL_PRODUCT_NAME)->setForeground(activeColor);
            stockItem->setForeground(activeColor);
        }
    }
    m_blockCellChanged = wasBlocked;
}

// ===== SELECTION =====

void InventoryWindow::onSelectionChanged()
{
    int row = getSelectedRow();
    bool hasSelection = (row >= 0);

    if (InventoryUIMap::editBtn)   InventoryUIMap::editBtn->setEnabled(hasSelection);
    if (InventoryUIMap::deleteBtn) InventoryUIMap::deleteBtn->setEnabled(hasSelection);

}

// ===== INLINE STOCK EDIT =====

void InventoryWindow::onCellDoubleClicked(int row, int column)
{
    if (m_currentTable != InventoryConstants::TABLE_PRODUCTS) return;
    if (column != InventoryConstants::COL_PRODUCT_STOCK) return;

    QTableWidgetItem* stockItem = InventoryUIMap::mainTable->item(row, InventoryConstants::COL_PRODUCT_STOCK);
    if (!stockItem || !(stockItem->flags() & Qt::ItemIsEditable)) return;
    // The table itself never edits on its own (no edit triggers): only this
    // cell opens, through the delegate's number box.
    m_isEditingStock = true;
    InventoryUIMap::mainTable->editItem(stockItem);
}

void InventoryWindow::onCellChanged(int row, int column)
{
    if (m_blockCellChanged) return;
    if (m_currentTable != InventoryConstants::TABLE_PRODUCTS) return;
    if (column != InventoryConstants::COL_PRODUCT_STOCK) return;

    QTableWidgetItem* stockItem = InventoryUIMap::mainTable->item(row, column);
    if (!stockItem || !(stockItem->flags() & Qt::ItemIsEditable)) return;

    // Colour changes (the out-of-stock flash) also land here: only a new
    // number is an edit.
    const int oldStock = stockItem->data(StockQuantityRole).toInt();
    bool ok = false;
    const int newStock = stockItem->text().toInt(&ok);
    auto restore = [this, stockItem, oldStock]() {
        m_blockCellChanged = true;
        stockItem->setText(QString::number(oldStock));
        m_blockCellChanged = false;
    };
    if (!ok || newStock < 0) {
        restore();
        AppHelpers::showWarning(this, AppHelpers::trMsg(InventoryConstants::MSG_STOCK_INVALID));
        return;
    }
    if (newStock == oldStock) return;

    const int productId = getProductIdFromRow(row);
    if (productId <= 0) { restore(); return; }

    // Absolute stock write (the out-of-stock flag is recomputed with it).
    if (!m_api->setStock(false, productId, newStock)) {
        qWarning() << "Failed to update stock:" << m_api->lastError();
        restore();
        AppHelpers::showError(this, tr("Failed to update stock.\n\n") + m_api->lastError());
        return;
    }
    qDebug() << "Stock updated for product" << productId << "to" << newStock;

    // Update this row in place: no reload, no jump.
    m_blockCellChanged = true;
    stockItem->setData(StockQuantityRole, newStock);
    stockItem->setData(StockKindRole,
                       newStock == InventoryConstants::OUT_OF_STOCK_VALUE ? QStringLiteral("out")
                       : newStock <= InventoryConstants::LOW_STOCK_THRESHOLD ? QStringLiteral("low")
                                                                              : QStringLiteral("normal"));
    stockItem->setText(QString::number(newStock));
    m_blockCellChanged = false;
    recountStockKpis();
    m_blockCellChanged = true;
    applyStockColors();
    m_blockCellChanged = false;
    updateFlashTimer();

    if (m_notifier)
        m_notifier->notifyChange(ChangeType::PRODUCT_CHANGED, productId);
}

void InventoryWindow::recountStockKpis()
{
    QTableWidget* t = InventoryUIMap::mainTable;
    if (!t) return;
    int out = 0, low = 0;
    for (int row = 0; row < t->rowCount(); ++row) {
        const QTableWidgetItem* item = t->item(row, InventoryConstants::COL_PRODUCT_STOCK);
        const QString kind = item ? item->data(StockKindRole).toString() : QString();
        if (kind == QLatin1String("out")) ++out;
        else if (kind == QLatin1String("low")) ++low;
    }
    updateKPIs(t->rowCount(), out, low);
    updateStockFilterButtons();
}

// ===== ADD / EDIT / DELETE ROUTING =====

void InventoryWindow::onAddClicked()
{
    switch (m_currentTable) {
    case InventoryConstants::TABLE_PRODUCTS:   addProduct();   break;
    case InventoryConstants::TABLE_CATEGORIES: addCategory();  break;
    case InventoryConstants::TABLE_DISCOUNTS:  addDiscount();  break;
    }
}

void InventoryWindow::onEditClicked()
{
    int row = getSelectedRow();
    if (row < 0) return;
    switch (m_currentTable) {
    case InventoryConstants::TABLE_PRODUCTS:   editProduct(row);   break;
    case InventoryConstants::TABLE_CATEGORIES: editCategory(row);  break;
    case InventoryConstants::TABLE_DISCOUNTS:  editDiscount(row);  break;
    }
}

void InventoryWindow::onDeleteClicked()
{
    int row = getSelectedRow();
    if (row < 0) return;

    if (m_session && !Permissions::can(
            m_api, m_session->username(), m_session->role(),
            AccessControl::PERMISSION_DELETE_PRODUCT)) {
        AppHelpers::showWarning(this, tr("You do not have permission to delete products."));
        return;
    }

    switch (m_currentTable) {
    case InventoryConstants::TABLE_PRODUCTS:   deleteProduct(row);   break;
    case InventoryConstants::TABLE_CATEGORIES: deleteCategory(row);  break;
    case InventoryConstants::TABLE_DISCOUNTS:  deleteDiscount(row);  break;
    }
}

void InventoryWindow::onLogoutClicked()
{
    if (!LogoutManager::confirmAndLogout(this, m_session->sessionId())) {
        return;
    }
    close();
    emit logoutRequested();
}

void InventoryWindow::handleNavigation(const QString& action)
{
    NotificationActions::Action parsed = NotificationActions::parse(action);

    AppHelpers::bringToFront(this);

    // "Inventory|show" is only a focus request.
    if (parsed.app.compare("Inventory", Qt::CaseInsensitive) != 0 ||
        parsed.flag.compare("show", Qt::CaseInsensitive) == 0)
        return;

    // Open the Products table and filter it to the notification's stock condition.
    if (m_currentTable != InventoryConstants::TABLE_PRODUCTS)
        onTabClicked(InventoryConstants::TABLE_PRODUCTS);
    InventoryUIMap::searchBox->clear();
    m_currentSearch.clear();
    loadCurrentTable();

    if (!parsed.flag.isEmpty())
        applyStockFilter(parsed.flag);
}

void InventoryWindow::applyStockFilter(const QString& flag)
{
    const bool showOut = flag.compare("out", Qt::CaseInsensitive) == 0;
    const int minStock = m_bell
        ? qBound(1, m_bell->manager()->settings().lowStockMin, 10)
        : InventoryConstants::LOW_STOCK_THRESHOLD;
    setStockFilter(showOut ? StockFilter::OutOfStock : StockFilter::LowStock,
                   minStock);
}

// ===== PRODUCTS CRUD =====

void InventoryWindow::addProduct()
{
    ProductDialog dialog(m_api, m_images, this);
    if (dialog.exec() == QDialog::Accepted) {
        loadProducts(m_currentSearch);
        if (m_notifier) m_notifier->notifyChange(ChangeType::PRODUCT_CHANGED, -1);
        AppHelpers::showInfo(this, AppHelpers::trMsg(InventoryConstants::MSG_PRODUCT_ADD_SUCCESS));
    }
}

void InventoryWindow::editProduct(int row)
{
    int productId = getProductIdFromRow(row);
    if (productId <= 0) return;

    ProductDialog dialog(m_api, m_images, productId, this);
    if (dialog.exec() == QDialog::Accepted) {
        loadProducts(m_currentSearch);
        if (m_notifier) m_notifier->notifyChange(ChangeType::PRODUCT_CHANGED, productId);
        AppHelpers::showInfo(this, AppHelpers::trMsg(InventoryConstants::MSG_PRODUCT_EDIT_SUCCESS));
    }
}

void InventoryWindow::deleteProduct(int row)
{
    int productId = getProductIdFromRow(row);
    if (productId <= 0) return;

    if (!AppHelpers::confirmAction(this, AppHelpers::trMsg(InventoryConstants::MSG_PRODUCT_DELETE_CONFIRM))) return;

    if (!m_api->remove("products", {{"id", productId}})) {
        AppHelpers::showError(this, tr("Failed to delete product: ") + m_api->lastError());
        return;
    }

    loadProducts(m_currentSearch);
    if (m_notifier) m_notifier->notifyChange(ChangeType::PRODUCT_CHANGED, productId);
    AppHelpers::showInfo(this, AppHelpers::trMsg(InventoryConstants::MSG_PRODUCT_DELETE_SUCCESS));
}

// ===== CATEGORIES CRUD =====

void InventoryWindow::addCategory()
{
    if (m_session && !Permissions::can(
            m_api, m_session->username(), m_session->role(),
            AccessControl::PERMISSION_ADD_CATEGORY)) {
        AppHelpers::showWarning(this, tr("You do not have permission to add categories."));
        return;
    }

    QString name;
    QString description;
    if (!categoryForm(this, false, name, description)) return;

    const int categoryId = m_api->insert("categories", {
        {"name", name},
        {"description", description.isEmpty() ? QVariant() : description},
        {"display_order", 0},
        {"visible_in_retail", 1},
        {"visible_in_cafe", 1}
    });

    if (categoryId < 0) {
        AppHelpers::showError(this, tr("Failed to add category: ") + m_api->lastError());
        return;
    }

    loadCategories(m_currentSearch);
    if (m_notifier) m_notifier->notifyChange(ChangeType::CATEGORY_CHANGED, categoryId);
    AppHelpers::showInfo(this, AppHelpers::trMsg(InventoryConstants::MSG_CATEGORY_ADD_SUCCESS));
}

void InventoryWindow::editCategory(int row)
{
    int categoryId = getCategoryIdFromRow(row);
    if (categoryId <= 0) return;

    const QString currentName = InventoryUIMap::mainTable->item(row, InventoryConstants::COL_CATEGORY_NAME)->text();
    const QString currentDesc = InventoryUIMap::mainTable->item(row, InventoryConstants::COL_CATEGORY_DESCRIPTION)->text();

    QString name = currentName;
    QString description = currentDesc;
    if (!categoryForm(this, true, name, description)) return;

    // Only name + description are written: the legacy client used to reset
    // display_order/visibility on every edit.
    if (!m_api->update("categories",
                       {{"name", name},
                        {"description", description.isEmpty() ? QVariant() : description}},
                       {{"id", categoryId}})) {
        AppHelpers::showError(this, tr("Failed to update category: ") + m_api->lastError());
        return;
    }

    loadCategories(m_currentSearch);
    if (m_notifier) m_notifier->notifyChange(ChangeType::CATEGORY_CHANGED, categoryId);
    AppHelpers::showInfo(this, AppHelpers::trMsg(InventoryConstants::MSG_CATEGORY_EDIT_SUCCESS));
}

void InventoryWindow::deleteCategory(int row)
{
    int categoryId = getCategoryIdFromRow(row);
    if (categoryId <= 0) return;

    const QList<DataRow> assigned =
        m_api->select("products", {{"category_id", categoryId}});
    const int count = assigned.size();

    QString message = count > 0
                          ? tr("This category is assigned to %1 product(s). They will become uncategorized. Continue?").arg(count)
                          : tr("Are you sure you want to delete this category?");

    if (!AppHelpers::confirmAction(this, message)) return;

    if (!m_api->remove("categories", {{"id", categoryId}})) {
        AppHelpers::showError(this, tr("Failed to delete category: ") + m_api->lastError());
        return;
    }

    loadCategories(m_currentSearch);
    if (m_notifier) m_notifier->notifyChange(ChangeType::CATEGORY_CHANGED, categoryId);
    AppHelpers::showInfo(this, AppHelpers::trMsg(InventoryConstants::MSG_CATEGORY_DELETE_SUCCESS));
}

// ===== DISCOUNTS CRUD =====

void InventoryWindow::addDiscount()
{
    if (m_session && !Permissions::can(
            m_api, m_session->username(), m_session->role(),
            AccessControl::PERMISSION_MANAGE_DISCOUNTS)) {
        AppHelpers::showWarning(this, tr("You do not have permission to manage discounts."));
        return;
    }

    DiscountValues values;
    if (!discountForm(this, false, values)) return;

    const int discountId = m_api->insert("discounts", {
        {"name", values.name},
        {"type", values.type},
        {"value", values.value},
        {"is_active", values.active ? 1 : 0},
        {"start_date", dateValue(values.start)},
        {"end_date", dateValue(values.end)}
    });

    if (discountId < 0) {
        AppHelpers::showError(this, tr("Failed to add discount: ") + m_api->lastError());
        return;
    }

    loadDiscounts(m_currentSearch);
    if (m_notifier) m_notifier->notifyChange(ChangeType::DISCOUNT_CHANGED, discountId);
    AppHelpers::showInfo(this, AppHelpers::trMsg(InventoryConstants::MSG_DISCOUNT_ADD_SUCCESS));
}

void InventoryWindow::editDiscount(int row)
{
    if (m_session && !Permissions::can(
            m_api, m_session->username(), m_session->role(),
            AccessControl::PERMISSION_MANAGE_DISCOUNTS)) {
        AppHelpers::showWarning(this, tr("You do not have permission to manage discounts."));
        return;
    }

    int discountId = getDiscountIdFromRow(row);
    if (discountId <= 0) return;

    // Load current values from the database
    const QList<DataRow> loadRows =
        m_api->select("discounts", {{"id", discountId}});
    if (loadRows.isEmpty()) return;
    const DataRow& saved = loadRows.first();

    const QString savedName      = saved.value("name").toString();
    const QString savedType      = saved.value("type").toString();
    const double  savedValue     = saved.value("value").toDouble();
    const bool    savedActive    = saved.value("is_active").toInt() != 0;
    const QString savedStartDate = saved.value("start_date").toString();
    const QString savedEndDate   = saved.value("end_date").toString();

    DiscountValues values;
    values.name = savedName;
    values.type = savedType;
    values.value = savedValue;
    values.active = savedActive;
    values.start = QDate::fromString(savedStartDate, "yyyy-MM-dd");
    values.end = QDate::fromString(savedEndDate, "yyyy-MM-dd");
    if (!discountForm(this, true, values)) return;

    if (!m_api->update("discounts",
                       {{"name", values.name},
                        {"type", values.type},
                        {"value", values.value},
                        {"is_active", values.active ? 1 : 0},
                        {"start_date", dateValue(values.start)},
                        {"end_date", dateValue(values.end)}},
                       {{"id", discountId}})) {
        AppHelpers::showError(this, tr("Failed to update discount: ") + m_api->lastError());
        return;
    }

    loadDiscounts(m_currentSearch);
    if (m_notifier) m_notifier->notifyChange(ChangeType::DISCOUNT_CHANGED, discountId);
    AppHelpers::showInfo(this, AppHelpers::trMsg(InventoryConstants::MSG_DISCOUNT_EDIT_SUCCESS));
}

void InventoryWindow::deleteDiscount(int row)
{
    if (m_session && !Permissions::can(
            m_api, m_session->username(), m_session->role(),
            AccessControl::PERMISSION_MANAGE_DISCOUNTS)) {
        AppHelpers::showWarning(this, tr("You do not have permission to manage discounts."));
        return;
    }

    int discountId = getDiscountIdFromRow(row);
    if (discountId <= 0) return;

    if (!AppHelpers::confirmAction(this, AppHelpers::trMsg(InventoryConstants::MSG_DISCOUNT_DELETE_CONFIRM))) return;

    if (!m_api->remove("discounts", {{"id", discountId}})) {
        AppHelpers::showError(this, tr("Failed to delete discount: ") + m_api->lastError());
        return;
    }

    loadDiscounts(m_currentSearch);
    if (m_notifier) m_notifier->notifyChange(ChangeType::DISCOUNT_CHANGED, discountId);
    AppHelpers::showInfo(this, AppHelpers::trMsg(InventoryConstants::MSG_DISCOUNT_DELETE_SUCCESS));
}

// ===== HELPERS =====

int InventoryWindow::getSelectedRow() const
{
    if (!InventoryUIMap::mainTable) return -1;
    QList<QTableWidgetItem*> selected = InventoryUIMap::mainTable->selectedItems();
    if (selected.isEmpty()) return -1;
    return selected.first()->row();
}

int InventoryWindow::getProductIdFromRow(int row) const
{
    QTableWidgetItem* item = InventoryUIMap::mainTable->item(row, InventoryConstants::COL_PRODUCT_ID);
    if (!item) return -1;
    return item->text().toInt();
}

int InventoryWindow::getCategoryIdFromRow(int row) const
{
    QTableWidgetItem* item = InventoryUIMap::mainTable->item(row, InventoryConstants::COL_CATEGORY_NAME);
    if (!item) return -1;
    return item->data(Qt::UserRole).toInt();
}

int InventoryWindow::getDiscountIdFromRow(int row) const
{
    QTableWidgetItem* item = InventoryUIMap::mainTable->item(row, InventoryConstants::COL_DISCOUNT_NAME);
    if (!item) return -1;
    return item->data(Qt::UserRole).toInt();
}
