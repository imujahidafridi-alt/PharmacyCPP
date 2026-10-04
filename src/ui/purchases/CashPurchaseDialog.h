#pragma once
#include "ui/components/AppModal.h"
#include "ui/components/AppDropdown.h"
#include "ui/components/AppTextInput.h"
#include "ui/components/MoneyInput.h"
#include "domain/Models.h"
#include <QSpinBox>
#include <QDateEdit>

namespace ui {

class CashPurchaseDialog : public AppModal {
    Q_OBJECT
public:
    explicit CashPurchaseDialog(QWidget* parent = nullptr);

private slots:
    void handleSave();

private:
    void loadItems();

    AppDropdown* m_itemCombo;
    QSpinBox* m_qtySpin;
    MoneyInput* m_costInput;
    AppTextInput* m_batchInput;
    QDateEdit* m_expiryEdit;
    AppTextInput* m_notesInput;
};

} // namespace ui
