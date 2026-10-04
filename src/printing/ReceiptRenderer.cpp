#include "printing/ReceiptRenderer.h"
#include "printing/ESCPOSPrinter.h"
#include "app/Configuration.h"

namespace printing {

QString ReceiptRenderer::renderPlainText(const domain::Sale& sale)
{
    const auto& cfg = app::Configuration::instance().settings();
    QString out;
    
    // Header
    if (cfg.printBismillahHeader) {
        out += "          بِسْمِ اللَّهِ الرَّحْمَٰنِ الرَّحِيمِ\n";
    }
    out += QString("%1\n").arg(cfg.storeName);
    out += QString("%1 | %2\n").arg(cfg.storePhone, cfg.storeAddress);
    if (!cfg.dslNumber.isEmpty() || !cfg.ntnNumber.isEmpty()) {
        out += QString("DSL: %1 | NTN: %2\n").arg(cfg.dslNumber, cfg.ntnNumber);
    }
    out += QString("------------------------------------------\n");
    out += QString("Bill #: %1\n").arg(sale.billNumber);
    out += QString("Date  : %1\n").arg(sale.createdAt.toString("dd-MMM-yyyy hh:mm AP"));
    out += QString("Cashier: %1\n").arg(sale.cashierName.isEmpty() ? "Counter" : sale.cashierName);
    out += QString("Customer: %1\n").arg(sale.customerName);
    out += QString("------------------------------------------\n");
    out += QString("%-20s %6s %7s %8s\n").arg("Item", "Qty", "Price", "Total");
    out += QString("------------------------------------------\n");

    for (const auto& it : sale.items) {
        QString name = it.itemName.left(20);
        out += QString::asprintf("%-20s %6d %7s %8s\n",
            name.toUtf8().constData(),
            it.displayQty,
            it.unitPrice.formatted(false).toUtf8().constData(),
            it.totalAmount.formatted(false).toUtf8().constData()
        );
    }

    out += QString("------------------------------------------\n");
    out += QString("%-28s %13s\n").arg("Subtotal:", sale.subtotal.formatted());
    if (sale.discount.isPositive()) {
        out += QString("%-28s %13s\n").arg("Discount:", sale.discount.formatted());
    }
    out += QString("%-28s %13s\n").arg("TOTAL:", sale.netTotal.formatted());
    out += QString("------------------------------------------\n");

    if (!sale.payments.empty()) {
        for (const auto& p : sale.payments) {
            QString pName = "Cash:";
            switch (p.type) {
                case domain::PaymentType::Cash: pName = "Cash Tendered:"; break;
                case domain::PaymentType::Udhaar: pName = "To Khata:"; break;
                case domain::PaymentType::Easypaisa: pName = "EasyPaisa:"; break;
                case domain::PaymentType::JazzCash: pName = "JazzCash:"; break;
                case domain::PaymentType::Card: pName = "Card:"; break;
                default: pName = "Bank/Other:"; break;
            }
            if (!p.reference.isEmpty()) pName += " (" + p.reference + ")";
            out += QString("%-28s %13s\n").arg(pName, p.amount.formatted());
        }
        if (sale.changeGiven.isPositive()) {
            out += QString("%-28s %13s\n").arg("Change Returned:", sale.changeGiven.formatted());
        }
    } else if (sale.paymentType == domain::PaymentType::Cash) {
        out += QString("%-28s %13s\n").arg("Cash Received:", sale.cashReceived.formatted());
        out += QString("%-28s %13s\n").arg("Change Returned:", sale.changeGiven.formatted());
    } else {
        out += QString("%-28s %13s\n").arg("Payment:", "Udhaar (Credit)");
    }

    out += QString("------------------------------------------\n");
    out += QString("%1\n").arg(cfg.receiptFooter);
    out += QString("Software by Pak Pharmacy POS\n");

    return out;
}

QByteArray ReceiptRenderer::renderEscPos(const domain::Sale& sale)
{
    const auto& cfg = app::Configuration::instance().settings();
    QByteArray ba;

    ba += ESCPOSPrinter::initPrinter();
    ba += ESCPOSPrinter::alignCenter();
    ba += ESCPOSPrinter::boldOn();
    ba += ESCPOSPrinter::doubleHeightOn();
    ba += cfg.storeName.toUtf8() + "\n";
    ba += ESCPOSPrinter::doubleHeightOff();
    ba += ESCPOSPrinter::boldOff();

    ba += QString("%1 | %2\n").arg(cfg.storePhone, cfg.storeAddress).toUtf8();
    ba += ESCPOSPrinter::horizontalLine(42);

    ba += ESCPOSPrinter::alignLeft();
    ba += QString("Bill #: %1\n").arg(sale.billNumber).toUtf8();
    ba += QString("Date  : %1\n").arg(sale.createdAt.toString("dd-MMM-yyyy hh:mm AP")).toUtf8();
    ba += QString("Customer: %1\n").arg(sale.customerName).toUtf8();
    ba += ESCPOSPrinter::horizontalLine(42);

    ba += QString("%-20s %5s %7s %8s\n").arg("Item", "Qty", "Price", "Total").toUtf8();
    ba += ESCPOSPrinter::horizontalLine(42);

    for (const auto& it : sale.items) {
        QString name = it.itemName.left(20);
        ba += QString::asprintf("%-20s %5d %7s %8s\n",
            name.toUtf8().constData(),
            it.displayQty,
            it.unitPrice.formatted(false).toUtf8().constData(),
            it.totalAmount.formatted(false).toUtf8().constData()
        ).toUtf8();
    }

    ba += ESCPOSPrinter::horizontalLine(42);
    ba += ESCPOSPrinter::boldOn();
    ba += QString("%-26s %15s\n").arg("TOTAL:", sale.netTotal.formatted()).toUtf8();
    ba += ESCPOSPrinter::boldOff();

    if (!sale.payments.empty()) {
        for (const auto& p : sale.payments) {
            QString pName = "Cash:";
            switch (p.type) {
                case domain::PaymentType::Cash: pName = "Cash Paid:"; break;
                case domain::PaymentType::Udhaar: pName = "To Khata:"; break;
                case domain::PaymentType::Easypaisa: pName = "EasyPaisa:"; break;
                case domain::PaymentType::JazzCash: pName = "JazzCash:"; break;
                case domain::PaymentType::Card: pName = "Card:"; break;
                default: pName = "Bank/Other:"; break;
            }
            if (!p.reference.isEmpty()) pName += " (" + p.reference + ")";
            ba += QString("%-26s %15s\n").arg(pName, p.amount.formatted()).toUtf8();
        }
        if (sale.changeGiven.isPositive()) {
            ba += QString("%-26s %15s\n").arg("Change:", sale.changeGiven.formatted()).toUtf8();
        }
    } else if (sale.paymentType == domain::PaymentType::Cash) {
        ba += QString("%-26s %15s\n").arg("Cash Paid:", sale.cashReceived.formatted()).toUtf8();
        ba += QString("%-26s %15s\n").arg("Change:", sale.changeGiven.formatted()).toUtf8();
    } else {
        ba += QString("%-26s %15s\n").arg("Payment:", "Udhaar").toUtf8();
    }

    ba += ESCPOSPrinter::horizontalLine(42);
    ba += ESCPOSPrinter::alignCenter();
    ba += cfg.receiptFooter.toUtf8() + "\n";
    ba += ESCPOSPrinter::lineFeed(3);
    ba += ESCPOSPrinter::cutPaper();
    ba += ESCPOSPrinter::openCashDrawer();

    return ba;
}

QString ReceiptRenderer::renderKhataStatement(const domain::Customer& customer, const std::vector<domain::CartItem>& /*recentItems*/)
{
    const auto& cfg = app::Configuration::instance().settings();
    QString out;
    if (cfg.printBismillahHeader) {
        out += "          بِسْمِ اللَّهِ الرَّحْمَٰنِ الرَّحِيمِ\n";
    }
    out += QString("%1\n").arg(cfg.storeName);
    out += QString("CUSTOMER KHATA STATEMENT\n");
    out += QString("------------------------------------------\n");
    out += QString("Customer: %1\n").arg(customer.name);
    out += QString("Phone   : %1\n").arg(customer.phone.isEmpty() ? "None" : customer.phone);
    out += QString("Current Baqaya: %1\n").arg(customer.baqaya.formatted());
    out += QString("------------------------------------------\n");
    out += QString("Software by Pak Pharmacy POS\n");
    return out;
}

QString ReceiptRenderer::renderZReportPlainText(const domain::CashSession& session)
{
    const auto& cfg = app::Configuration::instance().settings();
    QString out;
    if (cfg.printBismillahHeader) {
        out += "          بِسْمِ اللَّهِ الرَّحْمَٰنِ الرَّحِيمِ\n";
    }
    out += QString("==========================================\n");
    out += QString("           %1\n").arg(cfg.storeName);
    out += QString("        DAILY CASH CLOSING / Z-REPORT\n");
    out += QString("==========================================\n");
    out += QString("Counter   : Terminal #%1\n").arg(session.counterId);
    out += QString("Cashier   : %1\n").arg(session.cashierName);
    out += QString("Opened At : %1\n").arg(session.openedAt.toString("dd-MMM-yyyy hh:mm AP"));
    out += QString("Closed At : %1\n").arg(session.closedAt.has_value() ? session.closedAt->toString("dd-MMM-yyyy hh:mm AP") : "STILL ACTIVE");
    out += QString("------------------------------------------\n");
    out += QString("%-26s %15s\n").arg("Opening Float:", session.openingFloat.formatted());
    out += QString("%-26s %15s\n").arg("Cash Sales:", session.cashSales.formatted());
    if (session.cashReceivedCustomer.isPositive()) {
        out += QString("%-26s %15s\n").arg("Customer Payments:", session.cashReceivedCustomer.formatted());
    }
    if (session.cashReturns.isPositive()) {
        out += QString("%-26s %15s\n").arg("Cash Returns (Refunds):", QString("-%1").arg(session.cashReturns.formatted()));
    }
    out += QString("------------------------------------------\n");
    out += QString("%-26s %15s\n").arg("EXPECTED DRAWER CASH:", session.expectedCash.formatted());
    out += QString("%-26s %15s\n").arg("ACTUAL PHYSICAL COUNT:", session.actualCash.formatted());
    out += QString("------------------------------------------\n");

    QString diffStr;
    if (session.difference.isPositive()) {
        diffStr = QString("SURPLUS (FAZIL): +%1").arg(session.difference.formatted());
    } else if (session.difference.isNegative()) {
        diffStr = QString("SHORTAGE (KAMI): %1").arg(session.difference.formatted());
    } else {
        diffStr = "BALANCED (EXACT MATCH 0.00)";
    }
    out += QString("%1\n").arg(diffStr);

    if (!session.discrepancyNotes.trimmed().isEmpty()) {
        out += QString("Reason/Notes: %1\n").arg(session.discrepancyNotes);
    }

    out += QString("==========================================\n");
    out += QString("Cashier Signature: _______________________\n\n");
    out += QString("Manager Signature: _______________________\n");
    out += QString("Software by Pak Pharmacy POS\n");
    return out;
}

QByteArray ReceiptRenderer::renderZReportEscPos(const domain::CashSession& session)
{
    const auto& cfg = app::Configuration::instance().settings();
    QByteArray ba;

    ba += ESCPOSPrinter::initPrinter();
    ba += ESCPOSPrinter::alignCenter();
    ba += ESCPOSPrinter::boldOn();
    ba += ESCPOSPrinter::doubleHeightOn();
    ba += cfg.storeName.toUtf8() + "\n";
    ba += ESCPOSPrinter::doubleHeightOff();
    ba += "DAILY CASH CLOSING / Z-REPORT\n";
    ba += ESCPOSPrinter::boldOff();
    ba += ESCPOSPrinter::horizontalLine(42);

    ba += ESCPOSPrinter::alignLeft();
    ba += QString("Counter: Counter #%1 | Cashier: %2\n").arg(QString::number(session.counterId), session.cashierName).toUtf8();
    ba += QString("Opened : %1\n").arg(session.openedAt.toString("dd-MMM-yyyy hh:mm AP")).toUtf8();
    ba += QString("Closed : %1\n").arg(session.closedAt.has_value() ? session.closedAt->toString("dd-MMM-yyyy hh:mm AP") : "ACTIVE").toUtf8();
    ba += ESCPOSPrinter::horizontalLine(42);

    ba += QString("%-26s %15s\n").arg("Opening Float:", session.openingFloat.formatted()).toUtf8();
    ba += QString("%-26s %15s\n").arg("Cash Sales:", session.cashSales.formatted()).toUtf8();
    if (session.cashReceivedCustomer.isPositive()) {
        ba += QString("%-26s %15s\n").arg("Customer Payments:", session.cashReceivedCustomer.formatted()).toUtf8();
    }
    if (session.cashReturns.isPositive()) {
        ba += QString("%-26s %15s\n").arg("Cash Returns:", QString("-%1").arg(session.cashReturns.formatted())).toUtf8();
    }
    ba += ESCPOSPrinter::horizontalLine(42);

    ba += ESCPOSPrinter::boldOn();
    ba += QString("%-26s %15s\n").arg("EXPECTED CASH:", session.expectedCash.formatted()).toUtf8();
    ba += QString("%-26s %15s\n").arg("ACTUAL COUNTED:", session.actualCash.formatted()).toUtf8();
    ba += ESCPOSPrinter::boldOff();
    ba += ESCPOSPrinter::horizontalLine(42);

    if (session.difference.isPositive()) {
        ba += QString("STATUS: SURPLUS +%1\n").arg(session.difference.formatted()).toUtf8();
    } else if (session.difference.isNegative()) {
        ba += QString("STATUS: SHORTAGE %1\n").arg(session.difference.formatted()).toUtf8();
    } else {
        ba += "STATUS: EXACT BALANCED\n";
    }

    if (!session.discrepancyNotes.trimmed().isEmpty()) {
        ba += QString("Notes: %1\n").arg(session.discrepancyNotes).toUtf8();
    }

    ba += ESCPOSPrinter::horizontalLine(42);
    ba += "\nCashier Sign: ________________\n";
    ba += "Manager Sign: ________________\n\n";
    ba += ESCPOSPrinter::lineFeed(3);
    ba += ESCPOSPrinter::cutPaper();

    return ba;
}

} // namespace printing
