#pragma once
#include "ui/components/AppModal.h"
#include "ui/components/DataTable.h"
#include "domain/Models.h"

namespace ui {

class HoldBillsDialog : public AppModal {
    Q_OBJECT
public:
    explicit HoldBillsDialog(QWidget* parent = nullptr);

    QString selectedHoldId() const { return m_selectedHoldId; }

private slots:
    void handleResume();
    void handleDelete();

private:
    void loadHeldBills();

    DataTable* m_table;
    AppButton* m_deleteBtn;
    QString m_selectedHoldId;
};

} // namespace ui
