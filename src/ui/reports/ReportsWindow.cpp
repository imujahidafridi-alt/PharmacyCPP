#include "ui/reports/ReportsWindow.h"
#include "ui/reports/DayEndClosingDialog.h"
#include "ui/reports/InvoiceDetailsDialog.h"
#include "ui/returns/SaleReturnDialog.h"
#include "ui/components/AppSearchBox.h"
#include "ui/components/AppButton.h"
#include "ui/components/AppToast.h"
#include "database/DatabaseManager.h"
#include "core/Money.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QSqlQuery>

namespace ui {

ReportsWindow::ReportsWindow(QWidget* parent) : QWidget(parent)
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(14, 12, 14, 12);
    mainLayout->setSpacing(10);

    // Filter controls & Actions
    auto* filterBar = new QHBoxLayout();
    filterBar->setSpacing(8);

    auto* fromLbl = new QLabel("From:", this);
    fromLbl->setStyleSheet("font-size: 11px; font-weight: 700; color: #475569;");
    m_fromDateEdit = new QDateEdit(this);
    m_fromDateEdit->setDate(QDate::currentDate());
    m_fromDateEdit->setCalendarPopup(true);
    m_fromDateEdit->setFixedHeight(28);

    auto* toLbl = new QLabel("To:", this);
    toLbl->setStyleSheet("font-size: 11px; font-weight: 700; color: #475569;");
    m_toDateEdit = new QDateEdit(this);
    m_toDateEdit->setDate(QDate::currentDate());
    m_toDateEdit->setCalendarPopup(true);
    m_toDateEdit->setFixedHeight(28);

    auto* genBtn = new AppButton("Generate", AppButton::Variant::Primary, AppButton::Size::Medium, this);
    auto* closeDayBtn = new AppButton("Shift Closing (Z-Report)", AppButton::Variant::Secondary, AppButton::Size::Medium, this);

    filterBar->addWidget(fromLbl);
    filterBar->addWidget(m_fromDateEdit);
    filterBar->addWidget(toLbl);
    filterBar->addWidget(m_toDateEdit);
    filterBar->addWidget(genBtn);
    filterBar->addStretch();
    filterBar->addWidget(closeDayBtn);
    mainLayout->addLayout(filterBar);

    // Metric Summary Cards
    auto* cardsGrid = new QGridLayout();
    cardsGrid->setSpacing(8);
    auto createCard = [this](const QString& title, QLabel*& valLabel, const QString& color) -> QFrame* {
        auto* frame = new QFrame(this);
        frame->setStyleSheet("background-color: #FFFFFF; border: 1px solid #CBD5E1; border-radius: 4px; padding: 6px 10px;");
        auto* v = new QVBoxLayout(frame);
        v->setContentsMargins(2, 2, 2, 2);
        v->setSpacing(1);
        auto* t = new QLabel(title, frame);
        t->setStyleSheet("color: #64748B; font-size: 10px; font-weight: 700;");
        valLabel = new QLabel("Rs. 0", frame);
        valLabel->setStyleSheet(QString("color: %1; font-size: 15px; font-weight: 800;").arg(color));
        v->addWidget(t);
        v->addWidget(valLabel);
        return frame;
    };

    cardsGrid->addWidget(createCard("TOTAL SALES", m_totalSalesLabel, "#0F766E"), 0, 0);
    cardsGrid->addWidget(createCard("CASH SALES", m_cashSalesLabel, "#16A34A"), 0, 1);
    cardsGrid->addWidget(createCard("UDHAAR / CREDIT", m_creditSalesLabel, "#D97706"), 0, 2);
    cardsGrid->addWidget(createCard("EST. GROSS PROFIT", m_profitLabel, "#0F766E"), 0, 3);
    cardsGrid->addWidget(createCard("BILLS COUNT", m_billCountLabel, "#0F172A"), 0, 4);
    mainLayout->addLayout(cardsGrid);

    // Search bar for bills
    auto* searchRow = new QHBoxLayout();
    m_searchBox = new AppSearchBox("Search past bills by Bill #, Customer Name, or Payment Type...", AppSearchBox::Size::Medium, this);
    searchRow->addWidget(m_searchBox);
    mainLayout->addLayout(searchRow);

    // Sales breakdown table
    m_salesTable = new DataTable(this);
    m_salesTable->setupHeaders({"BILL #", "DATE & TIME", "CUSTOMER", "PAYMENT", "SUBTOTAL", "DISCOUNT", "NET TOTAL", "STATUS"});
    m_salesTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_salesTable->setColumnWidth(0, 110);
    m_salesTable->setColumnWidth(1, 140);
    m_salesTable->setColumnWidth(3, 85);
    m_salesTable->setColumnWidth(4, 90);
    m_salesTable->setColumnWidth(5, 80);
    m_salesTable->setColumnWidth(6, 100);
    m_salesTable->setColumnWidth(7, 85);
    mainLayout->addWidget(m_salesTable, 1);

    connect(genBtn, &QPushButton::clicked, this, &ReportsWindow::handleGenerate);
    connect(closeDayBtn, &QPushButton::clicked, this, &ReportsWindow::handleCloseDay);
    connect(m_searchBox, &AppSearchBox::searchDebounced, this, &ReportsWindow::handleSearchChanged);
    connect(m_salesTable, &QTableWidget::cellDoubleClicked, this, &ReportsWindow::handleBillDoubleClicked);

    refreshReports();
}

void ReportsWindow::refreshReports()
{
    handleGenerate();
}

void ReportsWindow::handleGenerate()
{
    m_salesTable->setRowCount(0);
    auto& dbMgr = database::DatabaseManager::instance();
    QSqlDatabase db = dbMgr.connection();
    if (!db.isOpen()) return;

    QDate fromDate = m_fromDateEdit->date();
    QDate toDate = m_toDateEdit->date();

    QString sql = R"(
        SELECT s.*, c.name AS customer_name
        FROM sales s
        LEFT JOIN customers c ON s.customer_id = c.id
        WHERE date(s.created_at) >= ? AND date(s.created_at) <= ?
        ORDER BY s.created_at DESC
    )";

    QSqlQuery q(db);
    q.prepare(sql);
    q.addBindValue(fromDate.toString("yyyy-MM-dd"));
    q.addBindValue(toDate.toString("yyyy-MM-dd"));

    if (!q.exec()) return;

    core::Money totalSales;
    core::Money cashSales;
    core::Money creditSales;
    int billCount = 0;

    while (q.next()) {
        int r = m_salesTable->rowCount();
        m_salesTable->insertRow(r);

        QString billNum = q.value("bill_number").toString();
        QDateTime dt = q.value("created_at").toDateTime();
        QString custName = q.value("customer_name").toString();
        QString pType = q.value("payment_type").toString();
        core::Money sub = core::Money::fromPaisa(q.value("subtotal_paisa").toLongLong());
        core::Money disc = core::Money::fromPaisa(q.value("discount_paisa").toLongLong());
        core::Money net = core::Money::fromPaisa(q.value("net_total_paisa").toLongLong());
        QString status = q.value("status").toString();

        m_salesTable->setItem(r, 0, new QTableWidgetItem(billNum));
        m_salesTable->setItem(r, 1, new QTableWidgetItem(dt.toString("dd-MMM-yyyy hh:mm AP")));
        m_salesTable->setItem(r, 2, new QTableWidgetItem(custName));
        m_salesTable->setItem(r, 3, new QTableWidgetItem(pType));
        m_salesTable->setItem(r, 4, new QTableWidgetItem(sub.formatted()));
        m_salesTable->setItem(r, 5, new QTableWidgetItem(disc.formatted()));
        m_salesTable->setItem(r, 6, new QTableWidgetItem(net.formatted()));
        m_salesTable->setItem(r, 7, new QTableWidgetItem(status));

        if (status == "Completed") {
            totalSales += net;
            if (pType == "Cash") cashSales += net;
            else creditSales += net;
            billCount++;
        }
    }

    // Profit query using batch purchase costs
    QSqlQuery profitQ(db);
    profitQ.prepare(R"(
        SELECT 
            COALESCE(SUM(si.total_amount_paisa), 0) AS total_revenue,
            COALESCE(SUM(si.atomic_qty * COALESCE(b.cost_price_paisa, i.purchase_cost_paisa, 0)), 0) AS total_cost
        FROM sale_items si
        JOIN sales s ON si.sale_id = s.id
        JOIN items i ON si.item_id = i.id
        LEFT JOIN batches b ON si.batch_id = b.id
        WHERE s.status = 'Completed' AND date(s.created_at) >= ? AND date(s.created_at) <= ?
    )");
    profitQ.addBindValue(fromDate.toString("yyyy-MM-dd"));
    profitQ.addBindValue(toDate.toString("yyyy-MM-dd"));

    core::Money grossProfit;
    if (profitQ.exec() && profitQ.next()) {
        int64_t rev = profitQ.value("total_revenue").toLongLong();
        int64_t cost = profitQ.value("total_cost").toLongLong();
        grossProfit = core::Money::fromPaisa(rev - cost);
    }

    m_totalSalesLabel->setText(totalSales.formatted());
    m_cashSalesLabel->setText(cashSales.formatted());
    m_creditSalesLabel->setText(creditSales.formatted());
    m_profitLabel->setText(grossProfit.formatted());
    m_billCountLabel->setText(QString::number(billCount));
}

void ReportsWindow::handleCloseDay()
{
    DayEndClosingDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        AppToast::showSuccess(this, "Day-End Cash Closing & Z-Report saved successfully!");
        refreshReports();
    }
}

void ReportsWindow::handleBillDoubleClicked(int row, int /*col*/)
{
    if (row < 0 || row >= m_salesTable->rowCount()) return;

    QString billNumber = m_salesTable->item(row, 0)->text().trimmed();
    if (billNumber.isEmpty()) return;

    InvoiceDetailsDialog dlg(billNumber, this);
    connect(&dlg, &InvoiceDetailsDialog::returnRequested, this, [this](const QString& billNum) {
        auto* returnModal = new QDialog(this);
        returnModal->setWindowTitle(QString("Process Return - %1").arg(billNum));
        returnModal->setMinimumSize(680, 480);
        auto* layout = new QVBoxLayout(returnModal);
        layout->setContentsMargins(0, 0, 0, 0);
        auto* retWidget = new SaleReturnDialog(returnModal);
        retWidget->loadBill(billNum);
        layout->addWidget(retWidget);
        returnModal->exec();
        refreshReports();
    });
    dlg.exec();
}

void ReportsWindow::handleSearchChanged(const QString& query)
{
    QString q = query.trimmed().toLower();
    for (int r = 0; r < m_salesTable->rowCount(); ++r) {
        if (q.isEmpty()) {
            m_salesTable->setRowHidden(r, false);
            continue;
        }

        QString bill = m_salesTable->item(r, 0) ? m_salesTable->item(r, 0)->text().toLower() : "";
        QString cust = m_salesTable->item(r, 2) ? m_salesTable->item(r, 2)->text().toLower() : "";
        QString payment = m_salesTable->item(r, 3) ? m_salesTable->item(r, 3)->text().toLower() : "";

        bool match = bill.contains(q) || cust.contains(q) || payment.contains(q);
        m_salesTable->setRowHidden(r, !match);
    }
}

} // namespace ui
