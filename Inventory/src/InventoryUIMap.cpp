#include "InventoryUIMap.h"
#include "ui_Invenorywindow.h"
#include "StyleManager.h"
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QSize>
#include <QDebug>

    // Static member initialization
    QComboBox*    InventoryUIMap::tableSelect = nullptr;
QLineEdit*    InventoryUIMap::searchBox   = nullptr;
QPushButton*  InventoryUIMap::addBtn      = nullptr;
QPushButton*  InventoryUIMap::editBtn     = nullptr;
QPushButton*  InventoryUIMap::deleteBtn   = nullptr;
QPushButton*  InventoryUIMap::logoutBtn   = nullptr;
QTableWidget* InventoryUIMap::mainTable   = nullptr;

void InventoryUIMap::initialize(Ui::MainWindow* ui)
{
    if (!ui) {
        qCritical() << "InventoryUIMap::initialize() called with null UI!";
        return;
    }

    // Map elements
    tableSelect = ui->tableSelect;
    searchBox   = ui->searchBox;
    addBtn      = ui->addBtn;
    editBtn     = ui->editBtn;
    deleteBtn   = ui->deleteBtn;
    logoutBtn   = ui->logoutBtn;
    mainTable   = ui->mainTable;

    // Initial states
    if (searchBox) {
        searchBox->setPlaceholderText("Search...");
    }

    if (editBtn) {
        editBtn->setEnabled(false);
        editBtn->setText("Edit");
    }

    if (deleteBtn) {
        deleteBtn->setEnabled(false);
        deleteBtn->setText("Delete");
    }

    if (addBtn) {
        addBtn->setText("Add");
    }

    if (tableSelect) {
        tableSelect->addItem("Products");
        tableSelect->addItem("Categories");
        tableSelect->addItem("Discounts");
    }

    if (logoutBtn) {
        logoutBtn->setText("");
        logoutBtn->setIcon(QIcon(StyleManager::iconPath("logout")));
        logoutBtn->setIconSize(QSize(22, 22));
        logoutBtn->setFixedSize(36, 36);
        logoutBtn->setToolTip("Logout");
    }

    qDebug() << "InventoryUIMap initialized successfully";
}
