#pragma once
#include "ui/components/AppModal.h"
#include "ui/components/DataTable.h"
#include "domain/Models.h"
#include <QLabel>

namespace ui {

class InvoiceDetailsDialog : public AppModal {
    Q_OBJECT
public:
    explicit InvoiceDetailsDialog(const QString& billNumber, QWidget* parent = nullptr);

signals:
    void returnRequested(const QString& billNumber);

private slots:
    void handleReprint();
    void handlePreview();
    void handleReturn();

private:
    void loadBillDetails(const QString& billNumber);

    domain::Sale m_sale;

    QLabel* m_billNumLabel{nullptr};
    QLabel* m_dateTimeLabel{nullptr};
    QLabel* m_customerLabel{nullptr};
    QLabel* m_paymentLabel{nullptr};
    QLabel* m_cashierLabel{nullptr};

    DataTable* m_itemsTable{nullptr};

    QLabel* m_subtotalLabel{nullptr};
    QLabel* m_discountLabel{nullptr};
    QLabel* m_totalLabel{nullptr};
};

} // namespace ui
