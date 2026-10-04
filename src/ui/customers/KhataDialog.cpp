#include "ui/customers/KhataDialog.h"
#include "ui/customers/ReceivePaymentDialog.h"
#include "services/LedgerService.h"
#include <QHeaderView>

namespace ui {

KhataDialog::KhataDialog(const domain::Customer& customer, QWidget* parent)
    : AppModal(QString("Customer Khata — %1").arg(customer.name), parent), m_customer(customer)
{
    resize(720, 480);
    setHeader(QString("Customer Khata: %1").arg(customer.name),
              customer.phone.isEmpty() ? "Detailed transaction ledger & history" : QString("Phone: %1 | Transaction ledger").arg(customer.phone),
              "📖");
    setConfirmButton("Receive Payment", AppButton::Variant::Primary);
    setCancelButton("Close (Esc)");

    // Outstanding Baqaya Card
    auto* baqayaCard = new QFrame(this);
    baqayaCard->setStyleSheet("background-color: #FEF2F2; border: 1px solid #FECACA; border-radius: 4px; padding: 6px 12px;");
    auto* bLayout = new QHBoxLayout(baqayaCard);
    bLayout->setContentsMargins(10, 4, 10, 4);

    auto* bTitle = new QLabel("OUTSTANDING BAQAYA:", baqayaCard);
    bTitle->setStyleSheet("font-size: 11px; font-weight: 700; color: #991B1B; background: transparent;");

    m_baqayaLabel = new QLabel(customer.baqaya.formatted(), baqayaCard);
    m_baqayaLabel->setStyleSheet("font-size: 18px; font-weight: 800; color: #DC2626; background: transparent;");

    bLayout->addWidget(bTitle);
    bLayout->addStretch();
    bLayout->addWidget(m_baqayaLabel);

    contentLayout()->addWidget(baqayaCard);

    m_table = new DataTable(this);
    m_table->setupHeaders({"Date", "Transaction Type", "Udhaar (+)", "Payment (-)", "Balance", "Notes"});
    m_table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);
    contentLayout()->addWidget(m_table);

    disconnect(confirmButton(), &QPushButton::clicked, this, &QDialog::accept);
    connect(confirmButton(), &QPushButton::clicked, this, &KhataDialog::handleReceivePayment);

    refreshLedger();
}

void KhataDialog::refreshLedger()
{
    m_table->setRowCount(0);
    auto custRes = services::LedgerService::instance().getCustomerById(m_customer.id);
    if (custRes.isOk()) {
        m_customer = custRes.value();
        m_baqayaLabel->setText(m_customer.baqaya.formatted());
    }

    auto entriesRes = services::LedgerService::instance().getCustomerLedger(m_customer.id, 100);
    if (entriesRes.isErr()) return;

    for (const auto& e : entriesRes.value()) {
        int r = m_table->rowCount();
        m_table->insertRow(r);
        m_table->setItem(r, 0, new QTableWidgetItem(e.timestamp.toString("dd-MMM-yyyy hh:mm AP")));
        m_table->setItem(r, 1, new QTableWidgetItem(e.referenceType));
        m_table->setItem(r, 2, new QTableWidgetItem(e.debit.isPositive() ? e.debit.formatted() : "-"));
        m_table->setItem(r, 3, new QTableWidgetItem(e.credit.isPositive() ? e.credit.formatted() : "-"));
        m_table->setItem(r, 4, new QTableWidgetItem(e.balanceAfter.formatted()));
        m_table->setItem(r, 5, new QTableWidgetItem(e.description));
    }
}

void KhataDialog::handleReceivePayment()
{
    ReceivePaymentDialog dlg(m_customer, this);
    if (dlg.exec() == QDialog::Accepted) {
        refreshLedger();
    }
}

} // namespace ui
