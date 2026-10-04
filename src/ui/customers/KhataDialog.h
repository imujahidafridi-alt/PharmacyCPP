#pragma once
#include "ui/components/AppModal.h"
#include "ui/components/DataTable.h"
#include "domain/Models.h"

namespace ui {

class KhataDialog : public AppModal {
    Q_OBJECT
public:
    explicit KhataDialog(const domain::Customer& customer, QWidget* parent = nullptr);

private slots:
    void handleReceivePayment();
    void refreshLedger();

private:
    domain::Customer m_customer;

    QLabel* m_baqayaLabel;
    DataTable* m_table;
};

} // namespace ui
