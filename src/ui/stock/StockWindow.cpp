#include "ui/stock/StockWindow.h"
#include "ui/stock/StockCorrectionDialog.h"
#include "ui/components/StatusBadge.h"
#include "services/StockService.h"
#include "database/DatabaseManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QSqlQuery>

namespace ui {

StockWindow::StockWindow(QWidget* parent) : QWidget(parent)
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    auto* filterBar = new QHBoxLayout();
    m_searchInput = new AppSearchBox("Filter stock by item name, code, barcode...", AppSearchBox::Size::Medium, this);

    m_categoryCombo = new AppDropdown(AppDropdown::Size::Medium, this);
    m_categoryCombo->addItem("All Categories", 0);

    m_correctBtn = AppButton::secondary("Stock Correction", AppButton::Size::Medium, this);

    filterBar->addWidget(m_searchInput, 3);
    filterBar->addWidget(m_categoryCombo, 2);
    filterBar->addWidget(m_correctBtn, 1);
    mainLayout->addLayout(filterBar);

    m_table = new DataTable(this);
    m_table->setupHeaders({"Code", "Item Name", "Category", "Current Stock", "Min Alert", "Status", "Nearest Expiry", "Price"});
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    mainLayout->addWidget(m_table);

    connect(m_searchInput, &AppSearchBox::searchDebounced, this, &StockWindow::handleSearchChanged);
    connect(m_categoryCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &StockWindow::handleCategoryChanged);
    connect(m_correctBtn, &QPushButton::clicked, this, &StockWindow::handleCorrectStock);

    // Populate categories
    auto& dbMgr = database::DatabaseManager::instance();
    QSqlDatabase db = dbMgr.connection();
    if (db.isOpen()) {
        QSqlQuery q(db);
        if (q.exec("SELECT id, name FROM categories ORDER BY name ASC")) {
            while (q.next()) {
                m_categoryCombo->addItem(q.value(1).toString(), q.value(0).toInt());
            }
        }
    }

    refreshStock();
}

void StockWindow::refreshStock()
{
    m_table->setRowCount(0);
    QString filter = m_searchInput->text().trimmed();
    int catId = m_categoryCombo->currentData().toInt();

    auto res = services::StockService::instance().getStockOverview(filter, catId);
    if (res.isErr()) return;

    m_items = res.value();
    for (const auto& it : m_items) {
        int r = m_table->rowCount();
        m_table->insertRow(r);
        m_table->setItem(r, 0, new QTableWidgetItem(it.code));
        m_table->setItem(r, 1, new QTableWidgetItem(it.itemName));
        m_table->setItem(r, 2, new QTableWidgetItem(it.categoryName));
        m_table->setItem(r, 3, new QTableWidgetItem(QString::number(it.totalAtomicQty)));
        m_table->setItem(r, 4, new QTableWidgetItem(QString::number(it.minStock)));

        auto* badge = new StatusBadge();
        badge->setStatus(it.status);
        m_table->setCellWidget(r, 5, badge);

        QString expStr = it.nearestExpiry.has_value() ? it.nearestExpiry->toString("dd-MMM-yyyy") : "-";
        m_table->setItem(r, 6, new QTableWidgetItem(expStr));
        m_table->setItem(r, 7, new QTableWidgetItem(it.retailPrice.formatted()));
    }
}

void StockWindow::handleSearchChanged(const QString&)
{
    refreshStock();
}

void StockWindow::handleCategoryChanged(int)
{
    refreshStock();
}

void StockWindow::handleCorrectStock()
{
    int r = m_table->currentRow();
    if (r < 0 || r >= static_cast<int>(m_items.size())) {
        QMessageBox::information(this, "Select Item", "Please select an item from the list to correct stock.");
        return;
    }

    const auto& selected = m_items[r];
    // Find batches for item
    auto batchesRes = services::StockService::instance().getItemBatches(selected.itemId);
    int batchId = 0;
    if (batchesRes.isOk() && !batchesRes.value().empty()) {
        batchId = batchesRes.value().front().id;
    }

    StockCorrectionDialog dlg(selected.itemId, selected.itemName, selected.totalAtomicQty, batchId, this);
    if (dlg.exec() == QDialog::Accepted) {
        refreshStock();
    }
}

} // namespace ui
