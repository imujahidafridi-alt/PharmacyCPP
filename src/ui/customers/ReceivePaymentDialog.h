#pragma once
#include "ui/components/AppModal.h"
#include "ui/components/AppDropdown.h"
#include "ui/components/AppTextInput.h"
#include "ui/components/MoneyInput.h"
#include "domain/Models.h"
#include <QLabel>

namespace ui {

class ReceivePaymentDialog : public AppModal {
    Q_OBJECT
public:
    explicit ReceivePaymentDialog(const domain::Customer& customer, QWidget* parent = nullptr);

private slots:
    void handleSave();

private:
    domain::Customer m_customer;

    QLabel* m_baqayaLabel;
    MoneyInput* m_amountInput;
    QLabel* m_remainingLabel;
    AppDropdown* m_methodCombo;
    AppTextInput* m_notesInput;
};

} // namespace ui
