#include "ui/sale/HoldBillsDialog.h"
#include "services/SaleService.h"
#include <QHeaderView>
#include <QMessageBox>

namespace ui {

HoldBillsDialog::HoldBillsDialog(QWidget* parent)
    : AppModal("Held Bills (F8)", parent)
{
    resize(620, 360);
    setHeader("Parked / Held Bills (F8)", "Resume customer order or discard held sales", "⏸️");
    setConfirmButton("Resume Bill (Enter)", AppButton::Variant::Primary);

    m_table = new DataTable(this);
    m_table->setupHeaders({"Hold ID", "Customer", "Time", "Items", "Total"});
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    contentLayout()->addWidget(m_table);

    m_deleteBtn = AppButton::danger("Discard Bill", AppButton::Size::Medium, this);
    addFooterButton(m_deleteBtn, true);

    disconnect(confirmButton(), &QPushButton::clicked, this, &QDialog::accept);
    connect(confirmButton(), &QPushButton::clicked, this, &HoldBillsDialog::handleResume);
    connect(m_deleteBtn, &QPushButton::clicked, this, &HoldBillsDialog::handleDelete);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int, int) { handleResume(); });

    loadHeldBills();
}

void HoldBillsDialog::loadHeldBills()
{
    m_table->setRowCount(0);
    auto bills = services::SaleService::instance().getHeldBills();
    for (const auto& b : bills) {
        int r = m_table->rowCount();
        m_table->insertRow(r);
        m_table->setItem(r, 0, new QTableWidgetItem(b.holdId));
        m_table->setItem(r, 1, new QTableWidgetItem(b.customerName));
        m_table->setItem(r, 2, new QTableWidgetItem(b.holdTime.toString("hh:mm AP")));
        m_table->setItem(r, 3, new QTableWidgetItem(QString::number(b.items.size())));
        m_table->setItem(r, 4, new QTableWidgetItem(b.total.formatted()));
    }
}

void HoldBillsDialog::handleResume()
{
    int r = m_table->currentRow();
    if (r < 0) {
        QMessageBox::information(this, "Select Bill", "Please select a held bill to resume.");
        return;
    }
    m_selectedHoldId = m_table->item(r, 0)->text();
    accept();
}

void HoldBillsDialog::handleDelete()
{
    int r = m_table->currentRow();
    if (r < 0) {
        QMessageBox::information(this, "Select Bill", "Please select a held bill to discard.");
        return;
    }
    QString hid = m_table->item(r, 0)->text();
    services::SaleService::instance().removeHeldBill(hid);
    loadHeldBills();
}

} // namespace ui
