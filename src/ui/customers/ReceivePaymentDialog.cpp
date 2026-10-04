#include "ui/customers/ReceivePaymentDialog.h"
#include "services/LedgerService.h"
#include "app/AppContext.h"
#include <QFormLayout>
#include <QMessageBox>

namespace ui {

ReceivePaymentDialog::ReceivePaymentDialog(const domain::Customer& customer, QWidget* parent)
    : AppModal("Receive Customer Payment", parent), m_customer(customer)
{
    setFixedWidth(420);
    setHeader("Receive Customer Payment", QString("Customer Account: <b>%1</b>").arg(customer.name), "💰");
    setConfirmButton("Receive Payment (Enter)", AppButton::Variant::Primary);

    auto* formLayout = new QFormLayout();
    formLayout->setSpacing(10);

    m_baqayaLabel = new QLabel(customer.baqaya.formatted(), this);
    m_baqayaLabel->setStyleSheet("font-size: 15px; font-weight: 700; color: #DC2626;");
    formLayout->addRow("Current Baqaya:", m_baqayaLabel);

    m_amountInput = new MoneyInput(this);
    m_amountInput->setFixedHeight(28);
    m_amountInput->setValue(customer.baqaya);
    formLayout->addRow("Amount Received *:", m_amountInput);

    m_remainingLabel = new QLabel("Rs. 0.00", this);
    m_remainingLabel->setStyleSheet("font-size: 13px; font-weight: 600; color: #475569;");
    formLayout->addRow("Remaining Baqaya:", m_remainingLabel);

    m_methodCombo = new AppDropdown(AppDropdown::Size::Medium, this);
    m_methodCombo->addItem("Cash");
    m_methodCombo->addItem("Bank Transfer");
    m_methodCombo->addItem("Easypaisa");
    m_methodCombo->addItem("JazzCash");
    formLayout->addRow("Payment Method:", m_methodCombo);

    m_notesInput = new AppTextInput("Optional notes...", AppTextInput::Size::Medium, this);
    formLayout->addRow("Notes:", m_notesInput);

    contentLayout()->addLayout(formLayout);

    connect(m_amountInput, &MoneyInput::valueChanged, this, [this](core::Money amount) {
        core::Money rem = m_customer.baqaya - amount;
        m_remainingLabel->setText(rem.formatted());
    });

    disconnect(confirmButton(), &QPushButton::clicked, this, &QDialog::accept);
    connect(confirmButton(), &QPushButton::clicked, this, &ReceivePaymentDialog::handleSave);

    m_amountInput->setFocus();
    m_amountInput->selectAll();
}

void ReceivePaymentDialog::handleSave()
{
    core::Money amount = m_amountInput->value();
    if (amount <= core::Money(0)) {
        QMessageBox::warning(this, "Invalid Amount", "Please enter a valid amount greater than zero.");
        return;
    }

    int userId = app::AppContext::instance().currentUser().id;
    auto res = services::LedgerService::instance().recordCustomerPayment(
        m_customer.id,
        amount,
        m_methodCombo->currentText(),
        m_notesInput->text().trimmed(),
        userId
    );

    if (res.isErr()) {
        QMessageBox::critical(this, "Payment Error", res.error().userMessage());
        return;
    }

    QMessageBox::information(this, "Payment Received", QString("Successfully received %1 from %2.").arg(amount.formatted(), m_customer.name));
    accept();
}

} // namespace ui
