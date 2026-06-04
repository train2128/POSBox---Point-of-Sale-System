#include "UIMap.h"
#include "ui_RegisterWindow.h"
#include "Prefixes.h"
#include "RegisterConstants.h"
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QIcon>
#include "StyleManager.h"

// Static member initialization
QLabel* UIMap::userLabel = nullptr;
QPushButton* UIMap::ticketsBtn = nullptr;
QPushButton* UIMap::notificationBtn = nullptr;
QPushButton* UIMap::logoutBtn = nullptr;

QLineEdit* UIMap::searchBar = nullptr;
QComboBox* UIMap::filterBox = nullptr;
QPushButton* UIMap::customItemBtn = nullptr;
QGridLayout* UIMap::itemsGridLayout = nullptr;

QScrollArea* UIMap::cart = nullptr;
QVBoxLayout* UIMap::cartLayout = nullptr;
QLabel* UIMap::totalLabel = nullptr;
QPushButton* UIMap::openTicketsBtn = nullptr;
QPushButton* UIMap::discountBtn = nullptr;
QPushButton* UIMap::withdrawBtn = nullptr;
QPushButton* UIMap::confirmBtn = nullptr;
QPushButton* UIMap::cancelBtn = nullptr;

void UIMap::initialize(Ui::RegisterWindow* ui)
{
    if (!ui) {
        qCritical() << "UIMap::initialize() called with null UI!";
        return;
    }

    // Map top bar elements
    userLabel       = ui->userLabel;
    ticketsBtn      = ui->ticketsBtn;
    notificationBtn = ui->notificationBtn;
    logoutBtn       = ui->logoutBtn;

    // Map products section
    searchBar       = ui->searchBar;
    filterBox       = ui->filterBox;
    customItemBtn   = ui->customItemBtn;
    itemsGridLayout = ui->itemsGridLayout;

    // Map cart section
    cart          = ui->cart;
    cartLayout    = ui->cartLayout;
    totalLabel    = ui->totalLabel;
    openTicketsBtn = ui->openTicketsBtn;
    discountBtn   = ui->discountBtn;
    withdrawBtn   = ui->withdrawBtn;
    confirmBtn    = ui->confirmBtn;
    cancelBtn     = ui->cancelBtn;

    // Withdraw button styling
    if (withdrawBtn) {
        withdrawBtn->setText("Withdraw");
        withdrawBtn->setStyleSheet(
            "QPushButton { "
            "    background-color: #E74C3C; "
            "    color: white; "
            "    border: none; "
            "    border-radius: 6px; "
            "    padding: 8px; "
            "    font-weight: bold; "
            "} "
            "QPushButton:hover { "
            "    background-color: #C0392B; "
            "}");
    }

    // cartLayout — push items to the top, space between them
    if (cartLayout) {
        cartLayout->setSpacing(RegisterConstants::CART_ITEM_SPACING);
        cartLayout->setContentsMargins(4, 4, 4, 4);
        cartLayout->addStretch(1); // trailing stretch keeps items at top
    }

    // Ensure scroll areas and their internal viewport widgets are transparent
    // so the QSS background rules on the parent window show through correctly.
    if (ui->itemsScrollArea) {
        ui->itemsScrollArea->setFrameShape(QFrame::NoFrame);
    }

    if (ui->cart) {
        ui->cart->viewport()->setAutoFillBackground(false);
        ui->cart->widget()->setAutoFillBackground(false);
    }

    // ===== TOP BAR =====

    if (searchBar) {
        searchBar->setPlaceholderText("Search products or scan barcode...");
    }

    if (totalLabel) {
        totalLabel->setObjectName("totalLabel");
        totalLabel->setText(QString("Total: 0.000 %1").arg(AppConstants::CURRENCY));
        totalLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    }

    if (ticketsBtn) {
        ticketsBtn->setText("Tickets");
    }

    // Notification button — icon only, square
    if (notificationBtn) {
        notificationBtn->setText("");
        notificationBtn->setIcon(QIcon(StyleManager::iconPath("notification")));
        notificationBtn->setIconSize(QSize(22, 22));
        notificationBtn->setFixedSize(36, 36);
        notificationBtn->setToolTip("Notifications");
    }

    // Logout button — icon only, square
    if (logoutBtn) {
        logoutBtn->setText("");
        logoutBtn->setIcon(QIcon(StyleManager::iconPath("logout")));
        logoutBtn->setIconSize(QSize(22, 22));
        logoutBtn->setFixedSize(36, 36);
        logoutBtn->setToolTip("Logout");
    }

    // ===== PRODUCTS SECTION =====

    if (customItemBtn) {
        customItemBtn->setText("Custom Item");
    }

    // ===== CART SECTION =====

    if (openTicketsBtn) {
        openTicketsBtn->setText("Open Tickets");
    }

    // Discount button — icon only, square
    if (discountBtn) {
        discountBtn->setText("");
        discountBtn->setIcon(QIcon(StyleManager::iconPath("discount")));
        discountBtn->setIconSize(QSize(22, 22));
        discountBtn->setFixedSize(36, 36);
        discountBtn->setToolTip("Apply Discount");
    }

    // Confirm — green
    if (confirmBtn) {
        confirmBtn->setText("Confirm");
        confirmBtn->setProperty("btnStyle", "add");
        confirmBtn->style()->unpolish(confirmBtn);
        confirmBtn->style()->polish(confirmBtn);
    }

    // Cancel — red
    if (cancelBtn) {
        cancelBtn->setText("Cancel");
        cancelBtn->setProperty("btnStyle", "delete");
        cancelBtn->style()->unpolish(cancelBtn);
        cancelBtn->style()->polish(cancelBtn);
    }

    qDebug() << "UIMap initialized successfully";
}
