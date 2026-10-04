#include "ui/items/QuickAddItemDialog.h"
#include "database/DatabaseManager.h"
#include <QFormLayout>
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QUuid>

namespace ui {

QuickAddItemDialog::QuickAddItemDialog(const QString& initialCodeOrBarcode, QWidget* parent)
    : AppModal("Add New Item", parent)
{
    setFixedWidth(420);
    setHeader("Quick Add Item", "Add a new medicine or retail item to inventory", "📦");
    setConfirmButton("Save Item (Enter)", AppButton::Variant::Primary);

    auto* formLayout = new QFormLayout();
    formLayout->setSpacing(10);

    m_nameInput = new AppTextInput("e.g. Panadol 500mg or Lux Soap", AppTextInput::Size::Medium, this);
    formLayout->addRow("Item Name *:", m_nameInput);

    m_barcodeInput = new AppTextInput("", AppTextInput::Size::Medium, this);
    m_barcodeInput->setText(initialCodeOrBarcode);
    m_barcodeInput->setPlaceholderText("Barcode or system auto-code");
    formLayout->addRow("Barcode / Code:", m_barcodeInput);

    m_categoryCombo = new AppDropdown(AppDropdown::Size::Medium, this);
    formLayout->addRow("Category *:", m_categoryCombo);

    m_salePriceInput = new MoneyInput(this);
    m_salePriceInput->setFixedHeight(28);
    formLayout->addRow("Sale Price *:", m_salePriceInput);

    m_costPriceInput = new MoneyInput(this);
    m_costPriceInput->setFixedHeight(28);
    formLayout->addRow("Purchase Cost:", m_costPriceInput);

    contentLayout()->addLayout(formLayout);

    disconnect(confirmButton(), &QPushButton::clicked, this, &QDialog::accept);
    connect(confirmButton(), &QPushButton::clicked, this, &QuickAddItemDialog::handleSave);

    loadCategories();
    m_nameInput->setFocus();
}

void QuickAddItemDialog::loadCategories()
{
    auto& dbMgr = database::DatabaseManager::instance();
    QSqlDatabase db = dbMgr.connection();
    if (!db.isOpen()) return;

    QSqlQuery q(db);
    q.prepare("SELECT id, name FROM categories ORDER BY name ASC");
    if (q.exec()) {
        while (q.next()) {
            m_categoryCombo->addItem(q.value(1).toString(), q.value(0).toInt());
        }
    }
}

void QuickAddItemDialog::handleSave()
{
    QString name = m_nameInput->text().trimmed();
    if (name.isEmpty()) {
        m_nameInput->setError(true, "Please enter the Item Name.");
        m_nameInput->setFocus();
        return;
    }
    m_nameInput->setError(false);

    if (m_salePriceInput->value().isZero()) {
        QMessageBox::warning(this, "Required", "Please enter a valid Sale Price.");
        m_salePriceInput->setFocus();
        return;
    }

    auto& dbMgr = database::DatabaseManager::instance();
    QSqlDatabase db = dbMgr.connection();
    if (!db.isOpen()) return;

    QString code = m_barcodeInput->text().trimmed();
    if (code.isEmpty()) {
        code = QString("ITEM-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces).left(6).toUpper());
    }

    int catId = m_categoryCombo->currentData().toInt();
    int64_t salePaisa = m_salePriceInput->value().paisa();
    int64_t costPaisa = m_costPriceInput->value().paisa();

    QSqlQuery q(db);
    q.prepare(R"(
        INSERT INTO items (code, name, category_id, barcode, sale_price_paisa, purchase_cost_paisa, min_stock_alert, is_active)
        VALUES (?, ?, ?, ?, ?, ?, 10, 1)
    )");
    q.addBindValue(code);
    q.addBindValue(name);
    q.addBindValue(catId);
    q.addBindValue(code);
    q.addBindValue(salePaisa);
    q.addBindValue(costPaisa);

    if (!q.exec()) {
        QMessageBox::critical(this, "Error", core::AppError::fromSqlError(q.lastError().text()).userMessage());
        return;
    }

    int newId = q.lastInsertId().toInt();
    m_createdItem.id = newId;
    m_createdItem.code = code;
    m_createdItem.name = name;
    m_createdItem.barcode = code;
    m_createdItem.categoryId = catId;
    m_createdItem.categoryName = m_categoryCombo->currentText();
    m_createdItem.salePrice = m_salePriceInput->value();
    m_createdItem.purchaseCost = m_costPriceInput->value();

    accept();
}

} // namespace ui
