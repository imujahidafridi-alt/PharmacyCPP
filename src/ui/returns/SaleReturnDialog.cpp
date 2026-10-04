#include "ui/returns/SaleReturnDialog.h"
#include "services/SaleService.h"
#include "services/ReturnService.h"
#include "app/AppContext.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QSpinBox>

namespace ui {

SaleReturnDialog::SaleReturnDialog(QWidget* parent) : QWidget(parent)
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    // Bill search bar
    auto* searchBar = new QHBoxLayout();
    m_billInput = new QLineEdit(this);
    m_billInput->setPlaceholderText("Enter Original Bill # (e.g. BILL-20261003-0001)...");
    m_billInput->setFixedHeight(28);

    m_findBtn = new QPushButton("Find Bill", this);
    m_findBtn->setFixedHeight(28);
    m_findBtn->setCursor(Qt::PointingHandCursor);

    searchBar->addWidget(m_billInput, 4);
    searchBar->addWidget(m_findBtn, 1);
    mainLayout->addLayout(searchBar);

    m_billDetailsLabel = new QLabel("Enter bill number above to load items.", this);
    m_billDetailsLabel->setStyleSheet("font-size: 13px; font-weight: 600; color: #475569;");
    mainLayout->addWidget(m_billDetailsLabel);

    // Table of sold items with editable return spinbox
    m_itemsTable = new DataTable(this);
    m_itemsTable->setupHeaders({"#", "Item Name", "Original Qty", "Price", "Return Qty"});
    m_itemsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    mainLayout->addWidget(m_itemsTable);

    // Bottom action row
    auto* bottomRow = new QHBoxLayout();
    m_reasonInput = new QLineEdit(this);
    m_reasonInput->setPlaceholderText("Return reason (e.g. Customer bought wrong medicine)...");
    m_reasonInput->setFixedHeight(28);

    m_cashRefundCheck = new QCheckBox("Refund in Cash", this);
    m_cashRefundCheck->setChecked(true);

    m_returnBtn = new QPushButton("Process Return", this);
    m_returnBtn->setProperty("class", "primaryBtn");
    m_returnBtn->setFixedHeight(28);
    m_returnBtn->setCursor(Qt::PointingHandCursor);
    m_returnBtn->setEnabled(false);

    bottomRow->addWidget(m_reasonInput, 3);
    bottomRow->addWidget(m_cashRefundCheck, 1);
    bottomRow->addWidget(m_returnBtn, 1);
    mainLayout->addLayout(bottomRow);

    connect(m_findBtn, &QPushButton::clicked, this, &SaleReturnDialog::handleFindBill);
    connect(m_billInput, &QLineEdit::returnPressed, this, &SaleReturnDialog::handleFindBill);
    connect(m_returnBtn, &QPushButton::clicked, this, &SaleReturnDialog::handleProcessReturn);
}

void SaleReturnDialog::handleFindBill()
{
    loadBill(m_billInput->text().trimmed());
}

void SaleReturnDialog::loadBill(const QString& billNumber)
{
    m_billInput->setText(billNumber);
    QString bNum = billNumber.trimmed();
    if (bNum.isEmpty()) return;

    auto res = services::SaleService::instance().getSaleByBillNumber(bNum);
    if (res.isErr()) {
        QMessageBox::warning(this, "Bill Not Found", res.error().userMessage());
        m_itemsTable->setRowCount(0);
        m_returnBtn->setEnabled(false);
        return;
    }

    m_loadedSale = res.value();
    m_billDetailsLabel->setText(QString("Bill #%1 | Customer: %2 | Date: %3 | Total: %4")
                                .arg(m_loadedSale.billNumber, m_loadedSale.customerName,
                                     m_loadedSale.createdAt.toString("dd-MMM-yyyy"),
                                     m_loadedSale.netTotal.formatted()));

    m_itemsTable->setRowCount(0);
    for (size_t i = 0; i < m_loadedSale.items.size(); ++i) {
        const auto& it = m_loadedSale.items[i];
        int r = m_itemsTable->rowCount();
        m_itemsTable->insertRow(r);
        m_itemsTable->setItem(r, 0, new QTableWidgetItem(QString::number(r + 1)));
        m_itemsTable->setItem(r, 1, new QTableWidgetItem(it.itemName));
        m_itemsTable->setItem(r, 2, new QTableWidgetItem(QString::number(it.displayQty)));
        m_itemsTable->setItem(r, 3, new QTableWidgetItem(it.unitPrice.formatted()));

        auto* spin = new QSpinBox();
        spin->setRange(0, it.displayQty);
        spin->setValue(0);
        m_itemsTable->setCellWidget(r, 4, spin);
    }

    m_returnBtn->setEnabled(!m_loadedSale.items.empty());
}

void SaleReturnDialog::handleProcessReturn()
{
    QString reason = m_reasonInput->text().trimmed();
    if (reason.isEmpty()) {
        QMessageBox::warning(this, "Reason Required", "Please enter a reason for the return.");
        m_reasonInput->setFocus();
        return;
    }

    std::vector<services::ReturnItemRequest> returnItems;
    for (int r = 0; r < m_itemsTable->rowCount(); ++r) {
        auto* spin = qobject_cast<QSpinBox*>(m_itemsTable->cellWidget(r, 4));
        if (spin && spin->value() > 0) {
            const auto& origItem = m_loadedSale.items[r];
            int returnDisplayQty = spin->value();
            int returnAtomicQty = returnDisplayQty * origItem.atomicUnitsPerQty;

            services::ReturnItemRequest req;
            req.itemId = origItem.itemId;
            req.batchId = origItem.batchId;
            req.returnAtomicQty = returnAtomicQty;
            req.refundAmount = origItem.unitPrice * returnDisplayQty;
            returnItems.push_back(req);
        }
    }

    if (returnItems.empty()) {
        QMessageBox::warning(this, "No Items", "Please set Return Qty > 0 for at least one item.");
        return;
    }

    int userId = app::AppContext::instance().currentUser().id;
    bool cashRefund = m_cashRefundCheck->isChecked();
    auto res = services::ReturnService::instance().processSaleReturn(
        m_loadedSale.billNumber,
        returnItems,
        reason,
        cashRefund,
        userId
    );

    if (res.isErr()) {
        QMessageBox::critical(this, "Return Error", res.error().userMessage());
        return;
    }

    QMessageBox::information(this, "Return Completed", "Return processed successfully. Stock has been restored.");
    m_billInput->clear();
    m_itemsTable->setRowCount(0);
    m_billDetailsLabel->setText("Enter bill number above to load items.");
    m_reasonInput->clear();
    m_returnBtn->setEnabled(false);
}

} // namespace ui
