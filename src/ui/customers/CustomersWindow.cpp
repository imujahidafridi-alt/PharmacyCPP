#include "ui/customers/CustomersWindow.h"
#include "ui/customers/KhataDialog.h"
#include "ui/customers/ReceivePaymentDialog.h"
#include "services/LedgerService.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QInputDialog>

namespace ui {

CustomersWindow::CustomersWindow(QWidget* parent) : QWidget(parent)
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    auto* topBar = new QHBoxLayout();
    m_searchInput = new AppSearchBox("Search customer by name or phone...", AppSearchBox::Size::Medium, this);

    m_addBtn = AppButton::primary("+ Add Customer", AppButton::Size::Medium, this);
    m_khataBtn = AppButton::secondary("View Khata", AppButton::Size::Medium, this);
    m_paymentBtn = AppButton::primary("Receive Payment", AppButton::Size::Medium, this);

    topBar->addWidget(m_searchInput, 3);
    topBar->addWidget(m_addBtn, 1);
    topBar->addWidget(m_khataBtn, 1);
    topBar->addWidget(m_paymentBtn, 1);
    mainLayout->addLayout(topBar);

    m_table = new DataTable(this);
    m_table->setupHeaders({"ID", "Customer Name", "Phone", "Address", "Outstanding Baqaya"});
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    mainLayout->addWidget(m_table);

    connect(m_searchInput, &AppSearchBox::searchDebounced, this, &CustomersWindow::handleSearchChanged);
    connect(m_addBtn, &QPushButton::clicked, this, &CustomersWindow::handleAddCustomer);
    connect(m_khataBtn, &QPushButton::clicked, this, &CustomersWindow::handleViewKhata);
    connect(m_paymentBtn, &QPushButton::clicked, this, &CustomersWindow::handleReceivePayment);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int, int) { handleViewKhata(); });

    refreshCustomers();
}

void CustomersWindow::refreshCustomers()
{
    m_table->setRowCount(0);
    auto res = services::LedgerService::instance().searchCustomers(m_searchInput->text().trimmed());
    if (res.isErr()) return;

    m_customers = res.value();
    for (const auto& c : m_customers) {
        int r = m_table->rowCount();
        m_table->insertRow(r);
        m_table->setItem(r, 0, new QTableWidgetItem(QString::number(c.id)));
        m_table->setItem(r, 1, new QTableWidgetItem(c.name));
        m_table->setItem(r, 2, new QTableWidgetItem(c.phone));
        m_table->setItem(r, 3, new QTableWidgetItem(c.address));
        
        auto* baqayaItem = new QTableWidgetItem(c.baqaya.formatted());
        if (c.baqaya.isPositive()) {
            baqayaItem->setForeground(QBrush(QColor("#DC2626")));
            baqayaItem->setFont(QFont("Segoe UI", 10, QFont::Bold));
        }
        m_table->setItem(r, 4, baqayaItem);
    }
}

void CustomersWindow::handleSearchChanged(const QString&)
{
    refreshCustomers();
}

void CustomersWindow::handleAddCustomer()
{
    bool ok = false;
    QString name = QInputDialog::getText(this, "Add Customer", "Customer Name *:", QLineEdit::Normal, "", &ok);
    if (!ok || name.trimmed().isEmpty()) return;

    QString phone = QInputDialog::getText(this, "Add Customer", "Phone Number (optional):", QLineEdit::Normal, "", &ok);
    QString address = QInputDialog::getText(this, "Add Customer", "Address (optional):", QLineEdit::Normal, "", &ok);

    auto res = services::LedgerService::instance().createCustomer(name, phone, address);
    if (res.isErr()) {
        QMessageBox::critical(this, "Error", res.error().userMessage());
        return;
    }
    refreshCustomers();
}

void CustomersWindow::handleViewKhata()
{
    int r = m_table->currentRow();
    if (r < 0 || r >= static_cast<int>(m_customers.size())) {
        QMessageBox::information(this, "Select Customer", "Please select a customer to view their Khata.");
        return;
    }

    KhataDialog dlg(m_customers[r], this);
    dlg.exec();
    refreshCustomers();
}

void CustomersWindow::handleReceivePayment()
{
    int r = m_table->currentRow();
    if (r < 0 || r >= static_cast<int>(m_customers.size())) {
        QMessageBox::information(this, "Select Customer", "Please select a customer to receive payment.");
        return;
    }

    ReceivePaymentDialog dlg(m_customers[r], this);
    if (dlg.exec() == QDialog::Accepted) {
        refreshCustomers();
    }
}

} // namespace ui
