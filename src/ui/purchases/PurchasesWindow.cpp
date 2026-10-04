#include "ui/purchases/PurchasesWindow.h"
#include "ui/purchases/CashPurchaseDialog.h"
#include "services/PurchaseService.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>

namespace ui {

PurchasesWindow::PurchasesWindow(QWidget* parent) : QWidget(parent)
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    auto* topBar = new QHBoxLayout();
    m_newCashPurchaseBtn = new QPushButton("+ New Cash Purchase (Market)", this);
    m_newCashPurchaseBtn->setFixedHeight(28);
    m_newCashPurchaseBtn->setCursor(Qt::PointingHandCursor);

    topBar->addStretch();
    topBar->addWidget(m_newCashPurchaseBtn);
    mainLayout->addLayout(topBar);

    m_table = new DataTable(this);
    m_table->setupHeaders({"ID", "Date", "Source / Supplier", "Type", "Total Cost", "Paid", "Notes"});
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    mainLayout->addWidget(m_table);

    connect(m_newCashPurchaseBtn, &QPushButton::clicked, this, &PurchasesWindow::handleNewCashPurchase);

    refreshPurchases();
}

void PurchasesWindow::refreshPurchases()
{
    m_table->setRowCount(0);
    auto res = services::PurchaseService::instance().getRecentPurchases(50);
    if (res.isErr()) return;

    for (const auto& p : res.value()) {
        int r = m_table->rowCount();
        m_table->insertRow(r);
        m_table->setItem(r, 0, new QTableWidgetItem(QString::number(p.id)));
        m_table->setItem(r, 1, new QTableWidgetItem(p.createdAt.toString("dd-MMM-yyyy hh:mm AP")));
        m_table->setItem(r, 2, new QTableWidgetItem(p.supplierName.isEmpty() ? "Cash Purchase / Market" : p.supplierName));
        m_table->setItem(r, 3, new QTableWidgetItem(p.isCashMarketPurchase ? "Cash Market" : "Invoice"));
        m_table->setItem(r, 4, new QTableWidgetItem(p.totalCost.formatted()));
        m_table->setItem(r, 5, new QTableWidgetItem(p.amountPaid.formatted()));
        m_table->setItem(r, 6, new QTableWidgetItem(p.notes));
    }
}

void PurchasesWindow::handleNewCashPurchase()
{
    CashPurchaseDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        refreshPurchases();
    }
}

} // namespace ui
