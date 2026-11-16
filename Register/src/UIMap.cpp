#include <QCoreApplication>
#include "UIMap.h"
#include "Animations.h"
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
#include "LineIcons.h"
#include "StyleManager.h"

// Static member initialization
QLabel* UIMap::userLabel = nullptr;
QLabel* UIMap::incomeLabel = nullptr;
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
    incomeLabel     = ui->incomeLabel;
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

    const LineIcons::Colors colors = LineIcons::colors();

    if (withdrawBtn) {
        withdrawBtn->setText(QCoreApplication::translate("RegisterWindow", "Withdraw"));
        withdrawBtn->setIcon(LineIcons::icon("cash", colors.text, 18));
        Animations::enableGlare(withdrawBtn);
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
        searchBar->setPlaceholderText(QCoreApplication::translate("RegisterWindow", "Search products or scan barcode..."));
    }

    if (incomeLabel) {
        incomeLabel->setObjectName("incomeLabel");
        incomeLabel->setAlignment(Qt::AlignCenter);
        incomeLabel->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    }

    if (totalLabel) {
        totalLabel->setObjectName("totalLabel");
        totalLabel->setText(QString("%1 %2").arg(AppHelpers::formatAmount(0), AppConstants::CURRENCY));
        totalLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    }

    // Notification button — icon only, square
    if (notificationBtn) {
        notificationBtn->setText("");
        notificationBtn->setIcon(QIcon(StyleManager::iconPath("notification")));
        notificationBtn->setIconSize(QSize(22, 22));
        notificationBtn->setFixedSize(36, 36);
        notificationBtn->setToolTip(QCoreApplication::translate("RegisterWindow", "Notifications"));
    }

    // Logout button — icon only, square
    if (logoutBtn) {
        logoutBtn->setText("");
        logoutBtn->setIcon(QIcon(StyleManager::iconPath("logout")));
        logoutBtn->setIconSize(QSize(22, 22));
        logoutBtn->setFixedSize(36, 36);
        logoutBtn->setToolTip(QCoreApplication::translate("RegisterWindow", "Logout"));
    }

    // ===== PRODUCTS SECTION =====

    if (customItemBtn) {
        customItemBtn->setText(QCoreApplication::translate("RegisterWindow", "Custom Item"));
        customItemBtn->setIcon(LineIcons::icon("plus", colors.text, 16));
        Animations::enableGlare(customItemBtn);
    }

    // ===== CART SECTION =====

    if (openTicketsBtn) {
        openTicketsBtn->setText(QCoreApplication::translate("RegisterWindow", "Open Tickets"));
        openTicketsBtn->setIcon(LineIcons::icon("receipt", colors.text, 18));
        Animations::enableGlare(openTicketsBtn);
    }

    if (discountBtn) {
        discountBtn->setText(QCoreApplication::translate("RegisterWindow", "Discount"));
        discountBtn->setIcon(LineIcons::icon("tag", Qt::white, 18));
        discountBtn->setToolTip(QCoreApplication::translate("RegisterWindow", "Apply Discount"));
        Animations::enableGlare(discountBtn);
    }

    // Confirm — green
    if (confirmBtn) {
        confirmBtn->setText(QCoreApplication::translate("RegisterWindow", "Confirm"));
        confirmBtn->setProperty("btnStyle", "add");
        confirmBtn->style()->unpolish(confirmBtn);
        confirmBtn->style()->polish(confirmBtn);
        Animations::enableGlare(confirmBtn);
    }

    // Cancel — quieter than Confirm, so the two aren't mistaken at speed
    if (cancelBtn) {
        cancelBtn->setText(QCoreApplication::translate("RegisterWindow", "Cancel"));
        cancelBtn->setProperty("btnStyle", "outlineDanger");
        cancelBtn->style()->unpolish(cancelBtn);
        cancelBtn->style()->polish(cancelBtn);
        Animations::enableGlare(cancelBtn);
    }

    qDebug() << "UIMap initialized successfully";
}
