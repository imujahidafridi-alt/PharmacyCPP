#include "ui/components/ReceiptPreviewDialog.h"
#include "ui/components/AppToast.h"
#include "printing/ReceiptRenderer.h"
#include "printing/ESCPOSPrinter.h"
#include "app/Configuration.h"
#include <QVBoxLayout>
#include <QFontDatabase>

namespace ui {

ReceiptPreviewDialog::ReceiptPreviewDialog(const domain::Sale& sale, QWidget* parent)
    : AppModal(QString("Receipt: %1").arg(sale.billNumber), parent)
    , m_sale(sale)
{
    setHeader(QString("Receipt: %1").arg(sale.billNumber), "80mm Thermal Receipt Live Preview", "🧾");
    setupUI(printing::ReceiptRenderer::renderPlainText(sale));
}

ReceiptPreviewDialog::ReceiptPreviewDialog(const QString& receiptText, const QString& title, QWidget* parent)
    : AppModal(title, parent)
{
    setHeader(title, "80mm Thermal Slip Live Preview", "🧾");
    setupUI(receiptText);
}

void ReceiptPreviewDialog::setupUI(const QString& receiptText)
{
    m_rawText = receiptText;
    setFixedWidth(400);

    m_receiptDisplay = new QTextEdit(this);
    m_receiptDisplay->setReadOnly(true);
    m_receiptDisplay->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_receiptDisplay->setPlainText(receiptText);
    m_receiptDisplay->setFixedHeight(400);
    m_receiptDisplay->setStyleSheet(R"(
        QTextEdit {
            background-color: #FFFFFF;
            color: #0F172A;
            border: 1px solid #CBD5E1;
            border-radius: 4px;
            font-family: 'Consolas', 'Courier New', monospace;
            font-size: 11px;
            line-height: 1.25;
            padding: 10px;
        }
    )");

    contentLayout()->addWidget(m_receiptDisplay);

    setConfirmButton("Print (80mm)", AppButton::Variant::Primary);
    setCancelButton("Close");

    disconnect(confirmButton(), &QPushButton::clicked, this, &QDialog::accept);
    connect(confirmButton(), &QPushButton::clicked, this, &ReceiptPreviewDialog::handlePrint);
}

void ReceiptPreviewDialog::handlePrint()
{
    QString defaultPrinter = app::Configuration::instance().settings().defaultPrinterName;
    if (defaultPrinter.isEmpty()) {
        AppToast::showWarning(this, "No default receipt printer configured in Settings.");
        return;
    }

    QByteArray bytes;
    if (m_sale.has_value()) {
        bytes = printing::ReceiptRenderer::renderEscPos(*m_sale);
    } else {
        // Plain text to ESC/POS conversion
        bytes = printing::ESCPOSPrinter::initPrinter();
        bytes += m_rawText.toUtf8();
        bytes += printing::ESCPOSPrinter::lineFeed(3);
        bytes += printing::ESCPOSPrinter::cutPaper();
    }

    auto res = printing::ESCPOSPrinter::sendRawToPrinter(defaultPrinter, bytes);
    if (res.isErr()) {
        AppToast::showError(this, QString("Print error: %1").arg(res.error().userMessage()));
    } else {
        AppToast::showSuccess(this, "Receipt sent to printer.");
    }
}

} // namespace ui
