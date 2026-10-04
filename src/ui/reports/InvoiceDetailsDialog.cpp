#include "ui/reports/InvoiceDetailsDialog.h"
#include "ui/components/ReceiptPreviewDialog.h"
#include "ui/components/AppToast.h"
#include "services/SaleService.h"
#include "printing/ReceiptRenderer.h"
#include "printing/ESCPOSPrinter.h"
#include "app/Configuration.h"
#include "database/DatabaseManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QSqlQuery>

namespace ui {

InvoiceDetailsDialog::InvoiceDetailsDialog(const QString& billNumber, QWidget* parent)
    : AppModal(QString("Bill #%1").arg(billNumber), parent)
{
    setFixedWidth(640);
    setHeader(QString("Bill #%1").arg(billNumber), "Complete Invoice Breakdown & Reprint", "🧾");

    auto* rootLayout = new QVBoxLayout();
    rootLayout->setSpacing(10);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    // 1. Metadata Chip Card
    auto* metaCard = new QFrame(this);
    metaCard->setStyleSheet("background-color: #F8FAFC; border: 1px solid #CBD5E1; border-radius: 5px; padding: 10px;");
    auto* metaGrid = new QGridLayout(metaCard);
    metaGrid->setContentsMargins(4, 4, 4, 4);
    metaGrid->setSpacing(8);

    auto addField = [metaGrid](int r, int c, const QString& title, QLabel*& valLbl) {
        auto* v = new QVBoxLayout();
        v->setSpacing(1);
        auto* t = new QLabel(title);
        t->setStyleSheet("font-size: 10px; font-weight: 700; color: #64748B;");
        valLbl = new QLabel("-");
        valLbl->setStyleSheet("font-size: 12px; font-weight: 700; color: #0F172A;");
        v->addWidget(t);
        v->addWidget(valLbl);
        metaGrid->addLayout(v, r, c);
    };

    addField(0, 0, "BILL NUMBER", m_billNumLabel);
    addField(0, 1, "DATE & TIME", m_dateTimeLabel);
    addField(0, 2, "CASHIER", m_cashierLabel);
    addField(1, 0, "CUSTOMER", m_customerLabel);
    addField(1, 1, "PAYMENT METHOD", m_paymentLabel);

    rootLayout->addWidget(metaCard);

    // 2. Itemized Table
    m_itemsTable = new DataTable(this);
    m_itemsTable->setupHeaders({"#", "ITEM NAME", "QTY", "PRICE", "TOTAL", "BATCH", "EXPIRY"});
    m_itemsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_itemsTable->setColumnWidth(0, 32);
    m_itemsTable->setColumnWidth(2, 50);
    m_itemsTable->setColumnWidth(3, 75);
    m_itemsTable->setColumnWidth(4, 85);
    m_itemsTable->setColumnWidth(5, 75);
    m_itemsTable->setColumnWidth(6, 75);
    m_itemsTable->setFixedHeight(220);
    rootLayout->addWidget(m_itemsTable);

    // 3. Totals Strip
    auto* totalsCard = new QFrame(this);
    totalsCard->setStyleSheet("background-color: #0F172A; border-radius: 4px; padding: 8px 12px;");
    auto* totalsLayout = new QHBoxLayout(totalsCard);
    totalsLayout->setContentsMargins(6, 4, 6, 4);
    totalsLayout->setSpacing(16);

    m_subtotalLabel = new QLabel("Subtotal: Rs. 0", totalsCard);
    m_subtotalLabel->setStyleSheet("font-size: 12px; font-weight: 600; color: #94A3B8;");
    m_discountLabel = new QLabel("Discount: Rs. 0", totalsCard);
    m_discountLabel->setStyleSheet("font-size: 12px; font-weight: 600; color: #94A3B8;");
    
    auto* totTitle = new QLabel("NET TOTAL:", totalsCard);
    totTitle->setStyleSheet("font-size: 12px; font-weight: 700; color: #CBD5E1;");
    m_totalLabel = new QLabel("Rs. 0", totalsCard);
    m_totalLabel->setStyleSheet("font-size: 16px; font-weight: 800; color: #38BDF8;");

    totalsLayout->addWidget(m_subtotalLabel);
    totalsLayout->addWidget(m_discountLabel);
    totalsLayout->addStretch();
    totalsLayout->addWidget(totTitle);
    totalsLayout->addWidget(m_totalLabel);

    rootLayout->addWidget(totalsCard);

    contentLayout()->addLayout(rootLayout);

    setConfirmButton("Reprint (80mm)", AppButton::Variant::Primary);
    setCancelButton("Close");

    auto* previewBtn = new AppButton("Preview Slip", AppButton::Variant::Info, AppButton::Size::Medium, this);
    previewBtn->setFixedHeight(30);
    connect(previewBtn, &QPushButton::clicked, this, &InvoiceDetailsDialog::handlePreview);
    addFooterButton(previewBtn, true);

    auto* returnBtn = new AppButton("Sale Return", AppButton::Variant::Danger, AppButton::Size::Medium, this);
    returnBtn->setFixedHeight(30);
    connect(returnBtn, &QPushButton::clicked, this, &InvoiceDetailsDialog::handleReturn);
    addFooterButton(returnBtn, true);

    disconnect(confirmButton(), &QPushButton::clicked, this, &QDialog::accept);
    connect(confirmButton(), &QPushButton::clicked, this, &InvoiceDetailsDialog::handleReprint);

    loadBillDetails(billNumber);
}

void InvoiceDetailsDialog::loadBillDetails(const QString& billNumber)
{
    auto res = services::SaleService::instance().getSaleByBillNumber(billNumber);
    if (res.isErr()) {
        AppToast::showError(this, "Could not load bill details from database.");
        return;
    }

    m_sale = res.value();

    m_billNumLabel->setText(m_sale.billNumber);
    m_dateTimeLabel->setText(m_sale.createdAt.toString("dd-MMM-yyyy hh:mm AP"));
    m_cashierLabel->setText(m_sale.cashierName.isEmpty() ? "Counter" : m_sale.cashierName);
    m_customerLabel->setText(m_sale.customerName);
    m_paymentLabel->setText(m_sale.paymentType == domain::PaymentType::Cash ? "Cash" : "Udhaar (Credit)");

    m_subtotalLabel->setText(QString("Subtotal: %1").arg(m_sale.subtotal.formatted()));
    m_discountLabel->setText(QString("Discount: %1").arg(m_sale.discount.formatted()));
    m_totalLabel->setText(m_sale.netTotal.formatted());

    m_itemsTable->setRowCount(0);
    for (size_t i = 0; i < m_sale.items.size(); ++i) {
        const auto& it = m_sale.items[i];
        int r = m_itemsTable->rowCount();
        m_itemsTable->insertRow(r);

        auto makeItem = [](const QString& text, Qt::Alignment align = Qt::AlignLeft) {
            auto* item = new QTableWidgetItem(text);
            item->setTextAlignment(align | Qt::AlignVCenter);
            return item;
        };

        m_itemsTable->setItem(r, 0, makeItem(QString::number(i + 1), Qt::AlignCenter));
        m_itemsTable->setItem(r, 1, makeItem(it.itemName));
        m_itemsTable->setItem(r, 2, makeItem(QString::number(it.displayQty), Qt::AlignCenter));
        m_itemsTable->setItem(r, 3, makeItem(it.unitPrice.formatted(false), Qt::AlignRight));
        m_itemsTable->setItem(r, 4, makeItem(it.totalAmount.formatted(), Qt::AlignRight));
        m_itemsTable->setItem(r, 5, makeItem(it.batchNumber.isEmpty() ? "-" : it.batchNumber, Qt::AlignCenter));
        m_itemsTable->setItem(r, 6, makeItem(it.expiryDate.isValid() ? it.expiryDate.toString("MM/yyyy") : "-", Qt::AlignCenter));
    }
}

void InvoiceDetailsDialog::handleReprint()
{
    QString defaultPrinter = app::Configuration::instance().settings().defaultPrinterName;
    if (defaultPrinter.isEmpty()) {
        AppToast::showWarning(this, "No receipt printer configured. Please select printer in Settings.");
        return;
    }

    QByteArray escposBytes = printing::ReceiptRenderer::renderEscPos(m_sale);
    auto printResult = printing::ESCPOSPrinter::sendRawToPrinter(defaultPrinter, escposBytes);
    if (printResult.isErr()) {
        AppToast::showError(this, QString("Print error: %1").arg(printResult.error().userMessage()));
    } else {
        AppToast::showSuccess(this, QString("Reprint sent to %1").arg(defaultPrinter));
    }
}

void InvoiceDetailsDialog::handlePreview()
{
    ReceiptPreviewDialog dlg(m_sale, this);
    dlg.exec();
}

void InvoiceDetailsDialog::handleReturn()
{
    emit returnRequested(m_sale.billNumber);
    accept();
}

} // namespace ui
