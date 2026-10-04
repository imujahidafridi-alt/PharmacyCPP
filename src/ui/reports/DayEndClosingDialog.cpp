#include "ui/reports/DayEndClosingDialog.h"
#include "ui/components/ReceiptPreviewDialog.h"
#include "ui/components/AppToast.h"
#include "database/DatabaseManager.h"
#include "app/AppContext.h"
#include "app/Configuration.h"
#include "printing/ReceiptRenderer.h"
#include "printing/ESCPOSPrinter.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSqlQuery>
#include <QDateTime>

namespace ui {

DayEndClosingDialog::DayEndClosingDialog(QWidget* parent)
    : AppModal("Day-End Cash Closing", parent)
{
    setFixedWidth(520);
    setHeader("Day-End Cash Closing", "Shift Reconciliation & Z-Report", "💵");

    const auto& user = app::AppContext::instance().currentUser();
    int counterId = app::Configuration::instance().settings().activeCounterId;

    m_session.counterId = counterId;
    m_session.userId = user.id;
    m_session.cashierName = user.fullName.isEmpty() ? "Counter Cashier" : user.fullName;
    m_session.openedAt = QDateTime::currentDateTime();

    calculateExpectedCash();

    auto* rootLayout = new QVBoxLayout();
    rootLayout->setSpacing(10);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    // 1. Shift Summary Stats Card
    auto* statsCard = new QFrame(this);
    statsCard->setStyleSheet("background-color: #F8FAFC; border: 1px solid #CBD5E1; border-radius: 5px; padding: 10px;");
    auto* statsGrid = new QGridLayout(statsCard);
    statsGrid->setContentsMargins(4, 4, 4, 4);
    statsGrid->setSpacing(8);

    auto addStat = [statsGrid](int r, int c, const QString& title, QLabel*& valLbl, const QString& initVal, const QString& color = "#0F172A") {
        auto* v = new QVBoxLayout();
        v->setSpacing(2);
        auto* t = new QLabel(title);
        t->setStyleSheet("font-size: 10px; font-weight: 700; color: #64748B;");
        valLbl = new QLabel(initVal);
        valLbl->setStyleSheet(QString("font-size: 13px; font-weight: 700; color: %1;").arg(color));
        v->addWidget(t);
        v->addWidget(valLbl);
        statsGrid->addLayout(v, r, c);
    };

    addStat(0, 0, "OPENING FLOAT", m_openingFloatLabel, m_session.openingFloat.formatted(), "#0F766E");
    addStat(0, 1, "CASH SALES TODAY", m_cashSalesLabel, m_session.cashSales.formatted(), "#16A34A");
    addStat(1, 0, "KHATA COLLECTIONS", m_khataPaymentsLabel, m_session.cashReceivedCustomer.formatted(), "#0284C7");
    addStat(1, 1, "CASH RETURNS (REFUNDS)", m_cashReturnsLabel, QString("-%1").arg(m_session.cashReturns.formatted()), "#DC2626");

    rootLayout->addWidget(statsCard);

    // Expected Drawer Cash Banner
    auto* expBanner = new QFrame(this);
    expBanner->setStyleSheet("background-color: #0F172A; border-radius: 4px; padding: 8px 12px;");
    auto* expLayout = new QHBoxLayout(expBanner);
    expLayout->setContentsMargins(6, 4, 6, 4);
    auto* expTitle = new QLabel("EXPECTED CASH IN DRAWER:", expBanner);
    expTitle->setStyleSheet("font-size: 11px; font-weight: 700; color: #94A3B8;");
    m_expectedCashLabel = new QLabel(m_session.expectedCash.formatted(), expBanner);
    m_expectedCashLabel->setStyleSheet("font-size: 16px; font-weight: 800; color: #38BDF8;");
    expLayout->addWidget(expTitle);
    expLayout->addStretch();
    expLayout->addWidget(m_expectedCashLabel);
    rootLayout->addWidget(expBanner);

    // 2. Pakistani Currency Note Denominations Breakdown
    auto* notesGroup = new QFrame(this);
    notesGroup->setStyleSheet("background-color: #FFFFFF; border: 1px solid #E2E8F0; border-radius: 4px; padding: 8px;");
    auto* notesLayout = new QVBoxLayout(notesGroup);
    notesLayout->setContentsMargins(4, 2, 4, 2);
    notesLayout->setSpacing(6);

    auto* notesHeader = new QLabel("Currency Notes Counter (Optional Quick Breakdown):", notesGroup);
    notesHeader->setStyleSheet("font-size: 11px; font-weight: 700; color: #475569;");
    notesLayout->addWidget(notesHeader);

    auto* denomGrid = new QGridLayout();
    denomGrid->setSpacing(6);

    auto createNoteCounter = [this, denomGrid](int col, int noteVal, QSpinBox*& spin) {
        auto* v = new QVBoxLayout();
        v->setSpacing(1);
        auto* l = new QLabel(QString("Rs. %1").arg(noteVal));
        l->setStyleSheet("font-size: 10px; font-weight: 700; color: #64748B;");
        l->setAlignment(Qt::AlignCenter);
        spin = new QSpinBox(this);
        spin->setRange(0, 9999);
        spin->setFixedHeight(24);
        spin->setAlignment(Qt::AlignCenter);
        spin->setStyleSheet("font-size: 11px; font-weight: 600;");
        v->addWidget(l);
        v->addWidget(spin);
        denomGrid->addLayout(v, 0, col);
        connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), this, &DayEndClosingDialog::handleNoteCountChanged);
    };

    createNoteCounter(0, 5000, m_note5000);
    createNoteCounter(1, 1000, m_note1000);
    createNoteCounter(2, 500, m_note500);
    createNoteCounter(3, 100, m_note100);
    createNoteCounter(4, 50, m_note50);
    createNoteCounter(5, 20, m_note20);
    createNoteCounter(6, 10, m_note10);

    notesLayout->addLayout(denomGrid);
    rootLayout->addWidget(notesGroup);

    // 3. Actual Counted Cash & Discrepancy
    auto* actualRow = new QHBoxLayout();
    actualRow->setSpacing(8);

    auto* actLbl = new QLabel("Actual Physical Cash (Rs.):", this);
    actLbl->setStyleSheet("font-size: 11px; font-weight: 700; color: #1E293B;");
    m_actualCashInput = new AppTextInput(AppTextInput::Size::Medium, this);
    m_actualCashInput->setPlaceholderText("Enter physical cash count");
    m_actualCashInput->setText(QString::number(m_session.expectedCash.toRupees(), 'f', 0));
    actualRow->addWidget(actLbl);
    actualRow->addWidget(m_actualCashInput, 1);
    rootLayout->addLayout(actualRow);

    // Discrepancy Indicator Card
    m_discrepancyCard = new QFrame(this);
    m_discrepancyCard->setStyleSheet("background-color: #F0FDF4; border: 1.5px solid #16A34A; border-radius: 4px; padding: 6px 10px;");
    auto* discLayout = new QHBoxLayout(m_discrepancyCard);
    discLayout->setContentsMargins(4, 2, 4, 2);
    m_discrepancyLabel = new QLabel("STATUS: BALANCED (Rs. 0.00 Difference)", m_discrepancyCard);
    m_discrepancyLabel->setStyleSheet("font-size: 11px; font-weight: 700; color: #166534;");
    discLayout->addWidget(m_discrepancyLabel);
    rootLayout->addWidget(m_discrepancyCard);

    // Notes Input
    m_notesInput = new AppTextInput(AppTextInput::Size::Small, this);
    m_notesInput->setPlaceholderText("Explanation / reason for difference or handover notes...");
    rootLayout->addWidget(m_notesInput);

    contentLayout()->addLayout(rootLayout);

    setConfirmButton("Close Shift & Print Z-Report", AppButton::Variant::Primary);
    setCancelButton("Cancel");

    auto* prevBtn = new AppButton("Preview Z-Report", AppButton::Variant::Info, AppButton::Size::Medium, this);
    prevBtn->setFixedHeight(30);
    connect(prevBtn, &QPushButton::clicked, this, &DayEndClosingDialog::handlePreviewZReport);
    addFooterButton(prevBtn, true);

    disconnect(confirmButton(), &QPushButton::clicked, this, &QDialog::accept);
    connect(confirmButton(), &QPushButton::clicked, this, &DayEndClosingDialog::handleConfirmClose);

    connect(m_actualCashInput, &QLineEdit::textChanged, this, &DayEndClosingDialog::handleActualAmountChanged);

    updateDiscrepancyDisplay();
}

void DayEndClosingDialog::calculateExpectedCash()
{
    auto& dbMgr = database::DatabaseManager::instance();
    QSqlDatabase db = dbMgr.connection();
    if (!db.isOpen()) return;

    // Check for existing open session for this counter
    QSqlQuery q(db);
    q.prepare("SELECT * FROM cash_sessions WHERE counter_id = ? AND is_open = 1 ORDER BY id DESC LIMIT 1");
    q.addBindValue(m_session.counterId);
    if (q.exec() && q.next()) {
        m_session.id = q.value("id").toInt();
        m_session.openingFloat = core::Money::fromPaisa(q.value("opening_float_paisa").toLongLong());
        m_session.openedAt = q.value("opened_at").toDateTime();
    } else {
        // Default opening float is 5000 if not previously set
        m_session.openingFloat = core::Money::fromRupees(5000.0);
    }

    // Cash sales today
    QSqlQuery qSales(db);
    qSales.prepare(R"(
        SELECT COALESCE(SUM(cash_received_paisa - change_given_paisa), 0)
        FROM sales
        WHERE payment_type = 'Cash' AND status = 'Completed' AND date(created_at) = date('now')
    )");
    if (qSales.exec() && qSales.next()) {
        m_session.cashSales = core::Money::fromPaisa(qSales.value(0).toLongLong());
    }

    // Customer khata cash payments
    QSqlQuery qCust(db);
    qCust.prepare(R"(
        SELECT COALESCE(SUM(credit_paisa), 0)
        FROM customer_ledger
        WHERE reference_type = 'Payment' AND date(created_at) = date('now')
    )");
    if (qCust.exec() && qCust.next()) {
        m_session.cashReceivedCustomer = core::Money::fromPaisa(qCust.value(0).toLongLong());
    }

    // Cash returns refunded today
    QSqlQuery qRet(db);
    qRet.prepare(R"(
        SELECT COALESCE(SUM(refund_amount_paisa), 0)
        FROM sale_returns
        WHERE date(created_at) = date('now')
    )");
    if (qRet.exec() && qRet.next()) {
        m_session.cashReturns = core::Money::fromPaisa(qRet.value(0).toLongLong());
    }

    m_session.expectedCash = m_session.openingFloat + m_session.cashSales + m_session.cashReceivedCustomer - m_session.cashReturns;
    m_session.actualCash = m_session.expectedCash;
    m_session.difference = core::Money(0);
}

void DayEndClosingDialog::handleNoteCountChanged()
{
    m_updatingFromNotes = true;
    double total = (m_note5000->value() * 5000.0) +
                   (m_note1000->value() * 1000.0) +
                   (m_note500->value() * 500.0) +
                   (m_note100->value() * 100.0) +
                   (m_note50->value() * 50.0) +
                   (m_note20->value() * 20.0) +
                   (m_note10->value() * 10.0);

    m_actualCashInput->setText(QString::number(total, 'f', 0));
    m_updatingFromNotes = false;
    handleActualAmountChanged(m_actualCashInput->text());
}

void DayEndClosingDialog::handleActualAmountChanged(const QString& val)
{
    bool ok = false;
    double amt = val.trimmed().toDouble(&ok);
    if (!ok || amt < 0.0) amt = 0.0;

    m_session.actualCash = core::Money::fromRupees(amt);
    m_session.difference = m_session.actualCash - m_session.expectedCash;
    updateDiscrepancyDisplay();
}

void DayEndClosingDialog::updateDiscrepancyDisplay()
{
    if (m_session.difference.isZero()) {
        m_discrepancyCard->setStyleSheet("background-color: #F0FDF4; border: 1.5px solid #16A34A; border-radius: 4px; padding: 6px 10px;");
        m_discrepancyLabel->setText("STATUS: EXACT BALANCED (Rs. 0.00 Difference)");
        m_discrepancyLabel->setStyleSheet("font-size: 11px; font-weight: 700; color: #166534;");
    } else if (m_session.difference.isPositive()) {
        m_discrepancyCard->setStyleSheet("background-color: #ECFDF5; border: 1.5px solid #059669; border-radius: 4px; padding: 6px 10px;");
        m_discrepancyLabel->setText(QString("STATUS: SURPLUS (FAZIL): +%1").arg(m_session.difference.formatted()));
        m_discrepancyLabel->setStyleSheet("font-size: 11px; font-weight: 700; color: #047857;");
    } else {
        m_discrepancyCard->setStyleSheet("background-color: #FEF2F2; border: 1.5px solid #DC2626; border-radius: 4px; padding: 6px 10px;");
        m_discrepancyLabel->setText(QString("STATUS: SHORTAGE (KAMI): %1").arg(m_session.difference.formatted()));
        m_discrepancyLabel->setStyleSheet("font-size: 11px; font-weight: 700; color: #B91C1C;");
    }
}

void DayEndClosingDialog::handlePreviewZReport()
{
    m_session.discrepancyNotes = m_notesInput->text().trimmed();
    m_session.closedAt = QDateTime::currentDateTime();
    QString zText = printing::ReceiptRenderer::renderZReportPlainText(m_session);
    ReceiptPreviewDialog dlg(zText, "Z-Report Shift Summary Preview", this);
    dlg.exec();
}

void DayEndClosingDialog::handleConfirmClose()
{
    m_session.discrepancyNotes = m_notesInput->text().trimmed();
    m_session.closedAt = QDateTime::currentDateTime();
    m_session.isOpen = false;

    auto& dbMgr = database::DatabaseManager::instance();
    QSqlDatabase db = dbMgr.connection();
    if (!db.isOpen()) return;

    if (m_session.id > 0) {
        QSqlQuery q(db);
        q.prepare(R"(
            UPDATE cash_sessions
            SET cash_sales_paisa = ?, cash_received_paisa = ?, cash_returns_paisa = ?,
                expected_cash_paisa = ?, actual_cash_paisa = ?, difference_paisa = ?,
                discrepancy_notes = ?, is_open = 0, closed_at = CURRENT_TIMESTAMP
            WHERE id = ?
        )");
        q.addBindValue(m_session.cashSales.paisa());
        q.addBindValue(m_session.cashReceivedCustomer.paisa());
        q.addBindValue(m_session.cashReturns.paisa());
        q.addBindValue(m_session.expectedCash.paisa());
        q.addBindValue(m_session.actualCash.paisa());
        q.addBindValue(m_session.difference.paisa());
        q.addBindValue(m_session.discrepancyNotes);
        q.addBindValue(m_session.id);
        q.exec();
    } else {
        QSqlQuery q(db);
        q.prepare(R"(
            INSERT INTO cash_sessions (
                counter_id, user_id, opening_float_paisa, cash_sales_paisa,
                cash_received_paisa, cash_returns_paisa, expected_cash_paisa,
                actual_cash_paisa, difference_paisa, discrepancy_notes, is_open, closed_at
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 0, CURRENT_TIMESTAMP)
        )");
        q.addBindValue(m_session.counterId);
        q.addBindValue(m_session.userId);
        q.addBindValue(m_session.openingFloat.paisa());
        q.addBindValue(m_session.cashSales.paisa());
        q.addBindValue(m_session.cashReceivedCustomer.paisa());
        q.addBindValue(m_session.cashReturns.paisa());
        q.addBindValue(m_session.expectedCash.paisa());
        q.addBindValue(m_session.actualCash.paisa());
        q.addBindValue(m_session.difference.paisa());
        q.addBindValue(m_session.discrepancyNotes);
        q.exec();
    }

    // Print 80mm Z-Report automatically if printer configured
    QString defaultPrinter = app::Configuration::instance().settings().defaultPrinterName;
    if (!defaultPrinter.isEmpty()) {
        QByteArray zBytes = printing::ReceiptRenderer::renderZReportEscPos(m_session);
        printing::ESCPOSPrinter::sendRawToPrinter(defaultPrinter, zBytes);
    }

    accept();
}

} // namespace ui
