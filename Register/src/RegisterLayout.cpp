#include "RegisterWindow.h"
#include "ui_RegisterWindow.h"
#include "LineIcons.h"
#include "UIMap.h"
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace {

constexpr int EmptyIconSize = 52;

QLabel* roleLabel(const QString& text, const char* role)
{
    auto* label = new QLabel(text);
    label->setProperty("uiRole", role);
    return label;
}

QFrame* separator()
{
    auto* line = new QFrame;
    line->setProperty("uiSep", true);
    return line;
}

} // namespace

// Arranges the window's controls. Every control comes from the designer file
// and keeps its signals; only its place on screen changes.
void RegisterWindow::buildLayout()
{
    auto* root = new QWidget;
    auto* rootLayout = new QVBoxLayout(root);
    rootLayout->setContentsMargins(16, 12, 16, 12);
    rootLayout->setSpacing(10);

    // Top bar
    auto* top = new QHBoxLayout;
    top->setSpacing(10);
    UIMap::userLabel->setProperty("uiRole", "plain");
    top->addWidget(UIMap::userLabel);
    top->addWidget(UIMap::withdrawBtn);
    top->addWidget(UIMap::incomeLabel);
    top->addStretch(1);
    top->addWidget(UIMap::notificationBtn);
    top->addWidget(UIMap::logoutBtn);
    rootLayout->addLayout(top);

    // Products
    auto* products = new QVBoxLayout;
    products->setSpacing(10);
    auto* toolbar = new QHBoxLayout;
    toolbar->setSpacing(8);
    UIMap::filterBox->setMinimumWidth(170);
    toolbar->addWidget(UIMap::searchBar, 1);
    toolbar->addWidget(UIMap::filterBox);
    toolbar->addWidget(UIMap::customItemBtn);
    products->addLayout(toolbar);
    ui->gridLayout->setContentsMargins(0, 0, 0, 0);
    products->addWidget(ui->itemsScrollArea, 1);

    // Cart
    auto* cartPanel = new QFrame;
    cartPanel->setObjectName("cartPanel");  // its width is set per font size
    cartPanel->setProperty("uiCard", true);
    auto* cartColumn = new QVBoxLayout(cartPanel);
    cartColumn->setContentsMargins(14, 10, 14, 14);
    cartColumn->setSpacing(8);

    auto* cartHeader = new QHBoxLayout;
    cartHeader->addWidget(roleLabel(tr("Current sale"), "cardTitle"), 1);
    m_cartCountLabel = new QLabel;
    m_cartCountLabel->setObjectName("kpiPill");
    cartHeader->addWidget(m_cartCountLabel);
    cartColumn->addLayout(cartHeader);
    cartColumn->addWidget(separator());

    ui->cart->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->cart->setFrameShape(QFrame::NoFrame);
    ui->verticalLayout_6->setContentsMargins(0, 0, 0, 0);
    UIMap::cartLayout->setContentsMargins(0, 0, 0, 0);
    cartColumn->addWidget(ui->cart, 1);

    m_cartEmptyHint = new QWidget;
    auto* emptyColumn = new QVBoxLayout(m_cartEmptyHint);
    emptyColumn->setSpacing(4);
    emptyColumn->addStretch(1);
    auto* emptyIcon = new QLabel;
    emptyIcon->setAlignment(Qt::AlignCenter);
    emptyIcon->setPixmap(LineIcons::pixmap("receipt", LineIcons::colors().muted, EmptyIconSize));
    emptyColumn->addWidget(emptyIcon);
    auto* emptyTitle = roleLabel(tr("Cart is empty"), "cardTitle");
    emptyTitle->setAlignment(Qt::AlignCenter);
    emptyColumn->addWidget(emptyTitle);
    auto* emptyHelp = roleLabel(tr("Tap a product or scan a barcode"), "desc");
    emptyHelp->setAlignment(Qt::AlignCenter);
    emptyColumn->addWidget(emptyHelp);
    emptyColumn->addStretch(1);
    cartColumn->addWidget(m_cartEmptyHint, 1);

    cartColumn->addWidget(separator());

    // Subtotal and discount only appear when a discount applies.
    m_totalsDetail = new QWidget;
    auto* detail = new QGridLayout(m_totalsDetail);
    detail->setContentsMargins(0, 0, 0, 0);
    detail->setVerticalSpacing(2);
    m_subtotalLabel = roleLabel(QString(), "desc");
    m_discountLabel = new QLabel;
    m_discountLabel->setObjectName("cartDiscountValue");
    auto* discountCaption = new QLabel(tr("Discount"));
    discountCaption->setObjectName("cartDiscountValue");
    detail->addWidget(roleLabel(tr("Subtotal"), "desc"), 0, 0);
    detail->addWidget(m_subtotalLabel, 0, 1, Qt::AlignRight);
    detail->addWidget(discountCaption, 1, 0);
    detail->addWidget(m_discountLabel, 1, 1, Qt::AlignRight);
    cartColumn->addWidget(m_totalsDetail);

    auto* totalRow = new QHBoxLayout;
    totalRow->addWidget(roleLabel(tr("Total"), "cardTitle"), 0, Qt::AlignVCenter);
    totalRow->addWidget(UIMap::totalLabel, 1);
    cartColumn->addLayout(totalRow);

    auto* ticketRow = new QHBoxLayout;
    ticketRow->setSpacing(8);
    ticketRow->addWidget(UIMap::openTicketsBtn, 1);
    ticketRow->addWidget(UIMap::discountBtn);
    cartColumn->addLayout(ticketRow);

    auto* payRow = new QHBoxLayout;
    payRow->setSpacing(8);
    payRow->addWidget(UIMap::confirmBtn, 1);
    payRow->addWidget(UIMap::cancelBtn);
    cartColumn->addLayout(payRow);

    auto* body = new QHBoxLayout;
    body->setSpacing(12);
    body->addLayout(products, 3);
    body->addWidget(cartPanel, 1);
    rootLayout->addLayout(body, 1);

    // The designer layout stays hidden: every control now lives in the new one.
    QWidget* designed = takeCentralWidget();
    designed->setParent(this);
    designed->hide();
    root->setObjectName("centralwidget");
    setCentralWidget(root);
}
