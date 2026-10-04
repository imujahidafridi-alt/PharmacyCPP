#include "ui/suppliers/SuppliersWindow.h"
#include "services/LedgerService.h"
#include "app/AppContext.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QInputDialog>

namespace ui {

SuppliersWindow::SuppliersWindow(QWidget* parent) : QWidget(parent)
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    auto* topBar = new QHBoxLayout();
    m_searchInput = new AppSearchBox("Search suppliers by name or phone...", AppSearchBox::Size::Medium, this);

    m_addBtn = AppButton::primary("+ Add Supplier", AppButton::Size::Medium, this);
    m_payBtn = AppButton::secondary("Pay Supplier", AppButton::Size::Medium, this);

    topBar->addWidget(m_searchInput, 3);
    topBar->addWidget(m_addBtn, 1);
    topBar->addWidget(m_payBtn, 1);
    mainLayout->addLayout(topBar);

    m_table = new DataTable(this);
    m_table->setupHeaders({"ID", "Supplier Name", "Phone", "Address", "Amount Owed (Baqaya)"});
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    mainLayout->addWidget(m_table);

    connect(m_searchInput, &AppSearchBox::searchDebounced, this, &SuppliersWindow::handleSearchChanged);
    connect(m_addBtn, &QPushButton::clicked, this, &SuppliersWindow::handleAddSupplier);
    connect(m_payBtn, &QPushButton::clicked, this, &SuppliersWindow::handlePaySupplier);

    refreshSuppliers();
}

void SuppliersWindow::refreshSuppliers()
{
    m_table->setRowCount(0);
    auto res = services::LedgerService::instance().searchSuppliers(m_searchInput->text().trimmed());
    if (res.isErr()) return;

    m_suppliers = res.value();
    for (const auto& s : m_suppliers) {
        int r = m_table->rowCount();
        m_table->insertRow(r);
        m_table->setItem(r, 0, new QTableWidgetItem(QString::number(s.id)));
        m_table->setItem(r, 1, new QTableWidgetItem(s.name));
        m_table->setItem(r, 2, new QTableWidgetItem(s.phone));
        m_table->setItem(r, 3, new QTableWidgetItem(s.address));
        
        auto* baqayaItem = new QTableWidgetItem(s.baqaya.formatted());
        if (s.baqaya.isPositive()) {
            baqayaItem->setForeground(QBrush(QColor("#DC2626")));
            baqayaItem->setFont(QFont("Segoe UI", 10, QFont::Bold));
        }
        m_table->setItem(r, 4, baqayaItem);
    }
}

void SuppliersWindow::handleSearchChanged(const QString&)
{
    refreshSuppliers();
}

void SuppliersWindow::handleAddSupplier()
{
    bool ok = false;
    QString name = QInputDialog::getText(this, "Add Supplier", "Supplier / Company Name *:", QLineEdit::Normal, "", &ok);
    if (!ok || name.trimmed().isEmpty()) return;

    QString phone = QInputDialog::getText(this, "Add Supplier", "Phone Number (optional):", QLineEdit::Normal, "", &ok);
    QString address = QInputDialog::getText(this, "Add Supplier", "Address / Market (optional):", QLineEdit::Normal, "", &ok);

    auto res = services::LedgerService::instance().createSupplier(name, phone, address);
    if (res.isErr()) {
        QMessageBox::critical(this, "Error", res.error().userMessage());
        return;
    }
    refreshSuppliers();
}

void SuppliersWindow::handlePaySupplier()
{
    int r = m_table->currentRow();
    if (r < 0 || r >= static_cast<int>(m_suppliers.size())) {
        QMessageBox::information(this, "Select Supplier", "Please select a supplier to record payment.");
        return;
    }

    const auto& supp = m_suppliers[r];
    bool ok = false;
    double amount = QInputDialog::getDouble(this, "Pay Supplier",
                                           QString("Enter payment to %1 (Current Baqaya: %2):").arg(supp.name, supp.baqaya.formatted()),
                                           supp.baqaya.toRupees(), 1.0, 9999999.0, 2, &ok);
    if (ok && amount > 0) {
        QString notes = QInputDialog::getText(this, "Pay Supplier", "Payment notes (Check #, Online ref):", QLineEdit::Normal, "Cash Payment", &ok);
        int userId = app::AppContext::instance().currentUser().id;
        auto res = services::LedgerService::instance().recordSupplierPayment(supp.id, core::Money::fromRupees(amount), notes, userId);
        if (res.isErr()) {
            QMessageBox::critical(this, "Error", res.error().userMessage());
            return;
        }
        QMessageBox::information(this, "Success", "Payment recorded successfully.");
        refreshSuppliers();
    }
}

} // namespace ui
