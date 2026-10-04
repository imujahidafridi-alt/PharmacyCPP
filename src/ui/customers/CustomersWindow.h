#pragma once
#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include "ui/components/DataTable.h"
#include "ui/components/AppSearchBox.h"
#include "ui/components/AppButton.h"
#include "domain/Models.h"

namespace ui {

class CustomersWindow : public QWidget {
    Q_OBJECT
public:
    explicit CustomersWindow(QWidget* parent = nullptr);

    void refreshCustomers();

private slots:
    void handleAddCustomer();
    void handleViewKhata();
    void handleReceivePayment();
    void handleSearchChanged(const QString& text);

private:
    AppSearchBox* m_searchInput;
    DataTable* m_table;
    AppButton* m_addBtn;
    AppButton* m_khataBtn;
    AppButton* m_paymentBtn;

    std::vector<domain::Customer> m_customers;
};

} // namespace ui
