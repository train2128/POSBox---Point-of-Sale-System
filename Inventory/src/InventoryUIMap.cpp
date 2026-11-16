#include <QCoreApplication>
#include "InventoryUIMap.h"
#include "ui_Invenorywindow.h"
#include "StyleManager.h"
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QLabel>
#include <QSize>
#include <QDebug>

    // Static member initialization
    QLabel*    InventoryUIMap::headerTitle      = nullptr;
    QLabel*    InventoryUIMap::headerSubtitle   = nullptr;
    QPushButton* InventoryUIMap::productsCountLabel = nullptr;
    QPushButton* InventoryUIMap::outOfStockLabel  = nullptr;
    QPushButton* InventoryUIMap::lowStockLabel    = nullptr;
    QPushButton*  InventoryUIMap::productsTab   = nullptr;
    QPushButton*  InventoryUIMap::categoriesTab = nullptr;
    QPushButton*  InventoryUIMap::discountsTab  = nullptr;
    QLineEdit*    InventoryUIMap::searchBox     = nullptr;
    QPushButton*  InventoryUIMap::addBtn        = nullptr;
    QPushButton*  InventoryUIMap::editBtn       = nullptr;
    QPushButton*  InventoryUIMap::deleteBtn     = nullptr;
    QPushButton*  InventoryUIMap::notificationBtn = nullptr;
    QPushButton*  InventoryUIMap::logoutBtn     = nullptr;
    QLabel*    InventoryUIMap::footerLabel      = nullptr;
    QTableWidget* InventoryUIMap::mainTable     = nullptr;

void InventoryUIMap::initialize(Ui::MainWindow* ui)
{
    if (!ui) {
        qCritical() << "InventoryUIMap::initialize() called with null UI!";
        return;
    }

    // Map search + actions (from .ui file)
    searchBox     = ui->searchBox;
    addBtn        = ui->addBtn;
    editBtn       = ui->editBtn;
    deleteBtn     = ui->deleteBtn;
    notificationBtn = ui->notificationBtn;
    logoutBtn     = ui->logoutBtn;

    // Map main table (from .ui file)
    mainTable     = ui->mainTable;

    // Header, KPI, tab, and footer widgets are created programmatically
    // in InventoryWindow::setupUI() and their pointers are set there.

    // Initial states — header
    if (headerTitle) {
        headerTitle->setText(QCoreApplication::translate("InventoryWindow", "Inventory"));
        headerTitle->setObjectName("headerTitle");
    }
    if (headerSubtitle) {
        headerSubtitle->setText(QCoreApplication::translate("InventoryWindow", "Products, pricing and stock"));
        headerSubtitle->setObjectName("headerSubtitle");
    }

    // Initial states — KPI labels
    if (productsCountLabel) {
        productsCountLabel->setObjectName("kpiLabel");
    }
    if (outOfStockLabel) {
        outOfStockLabel->setObjectName("kpiLabelOut");
    }
    if (lowStockLabel) {
        lowStockLabel->setObjectName("kpiLabelLow");
    }

    // Initial states — tab buttons (pill style, mutually exclusive)
    if (productsTab) {
        productsTab->setText(QCoreApplication::translate("InventoryWindow", "Products"));
        productsTab->setObjectName("tabProducts");
        productsTab->setCheckable(true);
        productsTab->setChecked(true);
        productsTab->setProperty("tabActive", "true");
    }
    if (categoriesTab) {
        categoriesTab->setText(QCoreApplication::translate("InventoryWindow", "Categories"));
        categoriesTab->setObjectName("tabCategories");
        categoriesTab->setCheckable(true);
        categoriesTab->setChecked(false);
        categoriesTab->setProperty("tabActive", "false");
    }
    if (discountsTab) {
        discountsTab->setText(QCoreApplication::translate("InventoryWindow", "Discounts"));
        discountsTab->setObjectName("tabDiscounts");
        discountsTab->setCheckable(true);
        discountsTab->setChecked(false);
        discountsTab->setProperty("tabActive", "false");
    }

    // Initial states — search
    if (searchBox) {
        searchBox->setPlaceholderText(QCoreApplication::translate("InventoryWindow", "Search name, category, barcode..."));
    }

    // Initial states — action buttons
    if (editBtn) {
        editBtn->setEnabled(false);
        editBtn->setText(QCoreApplication::translate("InventoryWindow", "Edit"));
        editBtn->setObjectName("actionEdit");
    }

    if (deleteBtn) {
        deleteBtn->setEnabled(false);
        deleteBtn->setText(QCoreApplication::translate("InventoryWindow", "Delete"));
        deleteBtn->setObjectName("actionDelete");
    }

    if (addBtn) {
        addBtn->setText(QCoreApplication::translate("InventoryWindow", "Add"));
        addBtn->setObjectName("actionAdd");
    }

    if (notificationBtn) {
        notificationBtn->setText("");
        notificationBtn->setIcon(QIcon(StyleManager::iconPath("notification")));
        notificationBtn->setIconSize(QSize(22, 22));
        notificationBtn->setFixedSize(36, 36);
        notificationBtn->setToolTip(QCoreApplication::translate("InventoryWindow", "Notifications"));
    }

    if (logoutBtn) {
        logoutBtn->setText("");
        logoutBtn->setIcon(QIcon(StyleManager::iconPath("logout")));
        logoutBtn->setIconSize(QSize(22, 22));
        logoutBtn->setFixedSize(36, 36);
        logoutBtn->setToolTip(QCoreApplication::translate("InventoryWindow", "Logout"));
        logoutBtn->setObjectName("logoutBtn");
    }

    // Initial states — footer
    if (footerLabel) {
        footerLabel->setObjectName("footerLabel");
        footerLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    }

    qDebug() << "InventoryUIMap initialized successfully";
}
