#include "ui/items/ItemsWindow.h"
#include "ui/items/QuickAddItemDialog.h"
#include "database/DatabaseManager.h"
#include "core/Money.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QSqlQuery>

namespace ui {

ItemsWindow::ItemsWindow(QWidget* parent) : QWidget(parent)
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    // Filter Bar
    auto* filterBar = new QHBoxLayout();
    m_searchInput = new AppSearchBox("Search items by name, barcode, code, or generic formula...", AppSearchBox::Size::Medium, this);

    m_categoryCombo = new AppDropdown(AppDropdown::Size::Medium, this);
    m_categoryCombo->addItem("All Categories", 0);

    m_addBtn = AppButton::primary("+ Add New Item", AppButton::Size::Medium, this);

    filterBar->addWidget(m_searchInput, 3);
    filterBar->addWidget(m_categoryCombo, 2);
    filterBar->addWidget(m_addBtn, 1);
    mainLayout->addLayout(filterBar);

    // Table
    m_table = new DataTable(this);
    m_table->setupHeaders({"Code", "Item Name", "Category", "Barcode", "Sale Price", "Cost Price", "Min Alert"});
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    mainLayout->addWidget(m_table);

    connect(m_searchInput, &AppSearchBox::searchDebounced, this, &ItemsWindow::handleSearchChanged);
    connect(m_categoryCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ItemsWindow::handleCategoryFilter);
    connect(m_addBtn, &QPushButton::clicked, this, &ItemsWindow::handleAddItem);

    // Load Categories into filter
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

    refreshItems();
}

void ItemsWindow::refreshItems()
{
    m_table->setRowCount(0);
    auto& dbMgr = database::DatabaseManager::instance();
    QSqlDatabase db = dbMgr.connection();
    if (!db.isOpen()) return;

    QString sql = R"(
        SELECT i.code, i.name, c.name AS category_name, i.barcode,
               i.sale_price_paisa, i.purchase_cost_paisa, i.min_stock_alert
        FROM items i
        LEFT JOIN categories c ON i.category_id = c.id
        WHERE i.is_active = 1
    )";

    QVariantList binds;
    QString txt = m_searchInput->text().trimmed();
    if (!txt.isEmpty()) {
        sql += " AND (i.name LIKE ? OR i.code LIKE ? OR i.barcode LIKE ? OR i.generic_name LIKE ?)";
        QString w = "%" + txt + "%";
        binds << w << w << w << w;
    }

    int catId = m_categoryCombo->currentData().toInt();
    if (catId > 0) {
        sql += " AND i.category_id = ?";
        binds << catId;
    }

    sql += " ORDER BY i.name ASC";

    QSqlQuery q(db);
    q.prepare(sql);
    for (const auto& b : binds) q.addBindValue(b);

    if (q.exec()) {
        while (q.next()) {
            int r = m_table->rowCount();
            m_table->insertRow(r);
            m_table->setItem(r, 0, new QTableWidgetItem(q.value("code").toString()));
            m_table->setItem(r, 1, new QTableWidgetItem(q.value("name").toString()));
            m_table->setItem(r, 2, new QTableWidgetItem(q.value("category_name").toString()));
            m_table->setItem(r, 3, new QTableWidgetItem(q.value("barcode").toString()));
            m_table->setItem(r, 4, new QTableWidgetItem(core::Money::fromPaisa(q.value("sale_price_paisa").toLongLong()).formatted()));
            m_table->setItem(r, 5, new QTableWidgetItem(core::Money::fromPaisa(q.value("purchase_cost_paisa").toLongLong()).formatted()));
            m_table->setItem(r, 6, new QTableWidgetItem(q.value("min_stock_alert").toString()));
        }
    }
}

void ItemsWindow::handleAddItem()
{
    QuickAddItemDialog dlg("", this);
    if (dlg.exec() == QDialog::Accepted) {
        refreshItems();
    }
}

void ItemsWindow::handleSearchChanged(const QString&)
{
    refreshItems();
}

void ItemsWindow::handleCategoryFilter(int)
{
    refreshItems();
}

} // namespace ui
