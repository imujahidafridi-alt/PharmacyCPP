#include "ui/purchases/CashPurchaseDialog.h"
#include "services/PurchaseService.h"
#include "database/DatabaseManager.h"
#include "app/AppContext.h"
#include <QFormLayout>
#include <QMessageBox>
#include <QSqlQuery>

namespace ui {

CashPurchaseDialog::CashPurchaseDialog(QWidget* parent)
    : AppModal("Cash Purchase", parent)
{
    setFixedWidth(440);
    setHeader("Cash Purchase / Market Buy", "Record local market cash buy and batch intake", "🛒");
    setConfirmButton("Save Purchase (Enter)", AppButton::Variant::Primary);

    auto* formLayout = new QFormLayout();
    formLayout->setSpacing(10);

    m_itemCombo = new AppDropdown(AppDropdown::Size::Medium, this);
    formLayout->addRow("Item *:", m_itemCombo);

    m_qtySpin = new QSpinBox(this);
    m_qtySpin->setRange(1, 99999);
    m_qtySpin->setValue(10);
    m_qtySpin->setFixedHeight(28);
    formLayout->addRow("Quantity *:", m_qtySpin);

    m_costInput = new MoneyInput(this);
    m_costInput->setFixedHeight(28);
    formLayout->addRow("Unit Cost (Rs.) *:", m_costInput);

    m_batchInput = new AppTextInput("e.g. B204", AppTextInput::Size::Medium, this);
    formLayout->addRow("Batch #:", m_batchInput);

    m_expiryEdit = new QDateEdit(this);
    m_expiryEdit->setDate(QDate::currentDate().addYears(2));
    m_expiryEdit->setCalendarPopup(true);
    m_expiryEdit->setFixedHeight(28);
    formLayout->addRow("Expiry Date:", m_expiryEdit);

    m_notesInput = new AppTextInput("e.g. Bought from Shah Alam", AppTextInput::Size::Medium, this);
    formLayout->addRow("Source/Notes:", m_notesInput);

    contentLayout()->addLayout(formLayout);

    disconnect(confirmButton(), &QPushButton::clicked, this, &QDialog::accept);
    connect(confirmButton(), &QPushButton::clicked, this, &CashPurchaseDialog::handleSave);

    loadItems();
}

void CashPurchaseDialog::loadItems()
{
    auto& dbMgr = database::DatabaseManager::instance();
    QSqlDatabase db = dbMgr.connection();
    if (!db.isOpen()) return;

    QSqlQuery q(db);
    q.prepare("SELECT id, name, code, purchase_cost_paisa FROM items WHERE is_active = 1 ORDER BY name ASC");
    if (q.exec()) {
        while (q.next()) {
            int id = q.value(0).toInt();
            QString name = q.value(1).toString();
            QString code = q.value(2).toString();
            m_itemCombo->addItem(QString("%1 (%2)").arg(name, code), id);
        }
    }
}

void CashPurchaseDialog::handleSave()
{
    int itemId = m_itemCombo->currentData().toInt();
    if (itemId <= 0) {
        QMessageBox::warning(this, "Required", "Please select an item.");
        return;
    }

    if (m_costInput->value().isZero()) {
        QMessageBox::warning(this, "Required", "Please enter a valid purchase cost.");
        m_costInput->setFocus();
        return;
    }

    int qty = m_qtySpin->value();
    core::Money unitCost = m_costInput->value();
    core::Money totalCost = unitCost * qty;

    domain::Purchase purchase;
    purchase.isCashMarketPurchase = true;
    purchase.supplierId = 1; // Default Cash Purchase / Local Market
    purchase.totalCost = totalCost;
    purchase.amountPaid = totalCost;
    purchase.notes = m_notesInput->text().trimmed();

    domain::PurchaseItem pItem;
    pItem.itemId = itemId;
    pItem.atomicQty = qty;
    pItem.unitCost = unitCost;
    pItem.totalCost = totalCost;
    pItem.batchNumber = m_batchInput->text().trimmed().isEmpty() ? "MKT-BATCH" : m_batchInput->text().trimmed();
    pItem.expiryDate = m_expiryEdit->date();
    purchase.items.push_back(pItem);

    int userId = app::AppContext::instance().currentUser().id;
    auto res = services::PurchaseService::instance().recordPurchase(purchase, userId);
    if (res.isErr()) {
        QMessageBox::critical(this, "Error", res.error().userMessage());
        return;
    }

    QMessageBox::information(this, "Purchase Saved", "Market cash purchase recorded and stock updated.");
    accept();
}

} // namespace ui
