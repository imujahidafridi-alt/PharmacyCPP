#pragma once
#include "ui/components/AppModal.h"
#include "ui/components/AppTextInput.h"
#include "ui/components/AppDropdown.h"
#include "ui/components/MoneyInput.h"
#include "domain/Models.h"

namespace ui {

class QuickAddItemDialog : public AppModal {
    Q_OBJECT
public:
    explicit QuickAddItemDialog(const QString& initialCodeOrBarcode = QString(), QWidget* parent = nullptr);

    domain::Item createdItem() const { return m_createdItem; }

private slots:
    void handleSave();

private:
    void loadCategories();

    AppTextInput* m_nameInput;
    AppTextInput* m_barcodeInput;
    AppDropdown* m_categoryCombo;
    MoneyInput* m_salePriceInput;
    MoneyInput* m_costPriceInput;

    domain::Item m_createdItem;
};

} // namespace ui
