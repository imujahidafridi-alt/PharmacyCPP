#pragma once
#include <QDialog>
#include <QLabel>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <vector>
#include "core/Money.h"
#include "domain/Models.h"
#include "ui/components/MoneyInput.h"

namespace ui {

class PaymentDialog : public QDialog {
    Q_OBJECT
public:
    explicit PaymentDialog(core::Money totalAmount, domain::Customer currentCustomer, QWidget* parent = nullptr);

    domain::PaymentType selectedPaymentType() const;
    std::vector<domain::PaymentAllocation> paymentAllocations() const;
    core::Money cashReceived() const;
    core::Money changeGiven() const;
    int selectedCustomerId() const;

protected:
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void recalculateSettlement();
    void setPresetExactCash();
    void setPresetEasyPaisa();
    void setPresetJazzCash();
    void setPresetCard();
    void setPresetKhata();

private:
    core::Money m_totalAmount;
    domain::Customer m_customer;

    QLabel* m_totalLabel;
    
    // Split payment inputs
    MoneyInput* m_cashTenderedInput;
    QComboBox* m_digitalProviderCombo;
    MoneyInput* m_digitalAmountInput;
    QLineEdit* m_digitalRefInput;
    MoneyInput* m_khataAmountInput;

    QLabel* m_customerBaqayaLabel;
    QLabel* m_allocatedLabel;
    QLabel* m_remainingLabel;
    QLabel* m_changeLabel;
    
    QPushButton* m_confirmBtn;
};

} // namespace ui
