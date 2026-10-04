#include "ui/expiry/ExpiryWindow.h"
#include "ui/stock/StockCorrectionDialog.h"
#include "ui/components/StatusBadge.h"
#include "services/StockService.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>

namespace ui {

ExpiryWindow::ExpiryWindow(QWidget* parent) : QWidget(parent)
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    auto* topBar = new QHBoxLayout();
    m_filterCombo = new QComboBox(this);
    m_filterCombo->addItem("Next 30 Days", 30);
    m_filterCombo->addItem("Next 60 Days", 60);
    m_filterCombo->addItem("Next 90 Days", 90);
    m_filterCombo->addItem("Already Expired", 0);
    m_filterCombo->setCurrentIndex(1); // Default 60 days
    m_filterCombo->setFixedHeight(28);

    m_correctBtn = new QPushButton("Correct / Remove from Stock", this);
    m_correctBtn->setProperty("class", "secondaryBtn");
    m_correctBtn->setFixedHeight(28);
    m_correctBtn->setCursor(Qt::PointingHandCursor);

    topBar->addWidget(m_filterCombo, 2);
    topBar->addStretch(3);
    topBar->addWidget(m_correctBtn, 2);
    mainLayout->addLayout(topBar);

    m_table = new DataTable(this);
    m_table->setupHeaders({"Code", "Item Name", "Category", "Batch", "Expiry Date", "Batch Qty", "Status"});
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    mainLayout->addWidget(m_table);

    connect(m_filterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ExpiryWindow::handleFilterChanged);
    connect(m_correctBtn, &QPushButton::clicked, this, &ExpiryWindow::handleStockCorrection);

    refreshExpiryList();
}

void ExpiryWindow::refreshExpiryList()
{
    m_table->setRowCount(0);
    int days = m_filterCombo->currentData().toInt();

    auto res = services::StockService::instance().getExpiringItems(days);
    if (res.isErr()) return;

    m_items = res.value();
    for (const auto& it : m_items) {
        int r = m_table->rowCount();
        m_table->insertRow(r);
        m_table->setItem(r, 0, new QTableWidgetItem(it.code));
        m_table->setItem(r, 1, new QTableWidgetItem(it.itemName));
        m_table->setItem(r, 2, new QTableWidgetItem(it.categoryName));
        m_table->setItem(r, 3, new QTableWidgetItem(it.nearestBatch));
        m_table->setItem(r, 4, new QTableWidgetItem(it.nearestExpiry.has_value() ? it.nearestExpiry->toString("dd-MMM-yyyy") : "-"));
        m_table->setItem(r, 5, new QTableWidgetItem(QString::number(it.totalAtomicQty)));

        auto* badge = new StatusBadge();
        badge->setStatus(it.status);
        m_table->setCellWidget(r, 6, badge);
    }
}

void ExpiryWindow::handleFilterChanged(int)
{
    refreshExpiryList();
}

void ExpiryWindow::handleStockCorrection()
{
    int r = m_table->currentRow();
    if (r < 0 || r >= static_cast<int>(m_items.size())) {
        QMessageBox::information(this, "Select Item", "Please select an expiring item from the list.");
        return;
    }

    const auto& selected = m_items[r];
    auto batchesRes = services::StockService::instance().getItemBatches(selected.itemId);
    int batchId = 0;
    if (batchesRes.isOk() && !batchesRes.value().empty()) {
        batchId = batchesRes.value().front().id;
    }

    StockCorrectionDialog dlg(selected.itemId, selected.itemName, selected.totalAtomicQty, batchId, this);
    if (dlg.exec() == QDialog::Accepted) {
        refreshExpiryList();
    }
}

} // namespace ui
