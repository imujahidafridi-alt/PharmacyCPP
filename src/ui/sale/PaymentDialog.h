#pragma once
#include <QDialog>
#include <QLabel>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QButtonGroup>
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
    domain::Customer selectedCustomer() const { return m_customer; }

protected:
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void recalculateSettlement();
    void setPresetCash();
    void setPresetEasyPaisa();
    void setPresetJazzCash();
    void setPresetCard();
    void setPresetKhata();
    void setPresetSplit();

    void addCash(int amountRupees);
    void setCash(int amountRupees);
    void clearCash();

    void openCustomerSelection();

private:
    void updateModePills(int activeIndex);
    QString computeChangeBreakdown(core::Money change) const;

    core::Money m_totalAmount;
    domain::Customer m_customer;

    // Header & Info
    QLabel* m_totalLabel;
    QLabel* m_customerInfoLabel;
    QPushButton* m_changeCustomerBtn;

    // Mode Buttons
    QButtonGroup* m_modeGroup;
    QPushButton* m_btnCash;
    QPushButton* m_btnEasyPaisa;
    QPushButton* m_btnJazzCash;
    QPushButton* m_btnCard;
    QPushButton* m_btnKhata;
    QPushButton* m_btnSplit;

    // Quick Denomination Card
    QWidget* m_denominationCard;

    // Payment Section Rows
    QWidget* m_cashRow;
    MoneyInput* m_cashTenderedInput;

    QWidget* m_digitalRow;
    QComboBox* m_digitalProviderCombo;
    MoneyInput* m_digitalAmountInput;
    QLineEdit* m_digitalRefInput;

    QWidget* m_khataRow;
    MoneyInput* m_khataAmountInput;
    QLabel* m_khataPreviewLabel;

    // Allocation & Change Bar
    QLabel* m_allocatedLabel;
    QLabel* m_statusLabel;
    QLabel* m_changeLabel;
    QLabel* m_breakdownLabel;

    QPushButton* m_confirmBtn;
};

} // namespace ui
