#include "ui/sale/PaymentDialog.h"
#include "ui/components/AppButton.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QKeyEvent>
#include <algorithm>

namespace ui {

PaymentDialog::PaymentDialog(core::Money totalAmount, domain::Customer currentCustomer, QWidget* parent)
    : QDialog(parent), m_totalAmount(totalAmount), m_customer(currentCustomer)
{
    setWindowTitle("Settlement & Payment (F9)");
    setFixedWidth(520);
    setStyleSheet(R"(
        QDialog { background-color: #FFFFFF; }
        QLabel { font-size: 12px; }
        QPushButton.presetBtn {
            background-color: #F1F5F9;
            color: #1E293B;
            border: 1px solid #CBD5E1;
            border-radius: 4px;
            font-size: 11px;
            font-weight: 700;
            padding: 4px 8px;
        }
        QPushButton.presetBtn:hover {
            background-color: #E2E8F0;
            border-color: #94A3B8;
        }
    )");

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(18, 16, 18, 16);

    // 1. Total display
    auto* totalBox = new QFrame(this);
    totalBox->setObjectName("paymentTotalBox");
    totalBox->setStyleSheet("#paymentTotalBox { background-color: #F0FDFA; border: 1.5px solid #0F766E; border-radius: 6px; }");
    auto* totalLayout = new QVBoxLayout(totalBox);
    totalLayout->setSpacing(2);
    totalLayout->setContentsMargins(14, 10, 14, 10);

    auto* totalTitle = new QLabel("TOTAL PAYABLE (NET)", totalBox);
    totalTitle->setStyleSheet("color: #0F766E; font-weight: 700; font-size: 11px; letter-spacing: 0.5px; background: transparent;");

    m_totalLabel = new QLabel(totalAmount.formatted(), totalBox);
    m_totalLabel->setStyleSheet("color: #0F766E; font-size: 26px; font-weight: 800; background: transparent;");

    totalLayout->addWidget(totalTitle);
    totalLayout->addWidget(m_totalLabel);
    mainLayout->addWidget(totalBox);

    // 2. Quick 1-Click Settlement Preset Chips
    auto* presetLabel = new QLabel("1-Click Quick Settlement:", this);
    presetLabel->setStyleSheet("font-size: 11px; font-weight: 700; color: #64748B;");
    mainLayout->addWidget(presetLabel);

    auto* presetLayout = new QHBoxLayout();
    presetLayout->setSpacing(6);

    auto addPreset = [this, presetLayout](const QString& text, auto slot) {
        auto* btn = new QPushButton(text, this);
        btn->setProperty("class", "presetBtn");
        btn->setFixedHeight(28);
        btn->setCursor(Qt::PointingHandCursor);
        connect(btn, &QPushButton::clicked, this, slot);
        presetLayout->addWidget(btn);
        return btn;
    };

    addPreset("💵 Exact Cash (F1)", &PaymentDialog::setPresetExactCash);
    addPreset("📱 EasyPaisa (F2)", &PaymentDialog::setPresetEasyPaisa);
    addPreset("📲 JazzCash (F3)", &PaymentDialog::setPresetJazzCash);
    addPreset("💳 Card (F4)", &PaymentDialog::setPresetCard);
    
    if (m_customer.id > 1) {
        addPreset("📝 100% Khata (F5)", &PaymentDialog::setPresetKhata);
    }

    mainLayout->addLayout(presetLayout);

    // 3. Customer Context
    m_customerBaqayaLabel = new QLabel(this);
    if (m_customer.id > 1) {
        m_customerBaqayaLabel->setText(
            QString("Customer: <b>%1</b> &nbsp;|&nbsp; Current Baqaya: <b>%2</b>")
                .arg(m_customer.name, m_customer.baqaya.formatted())
        );
        m_customerBaqayaLabel->setStyleSheet("color: #1E293B; font-size: 11px; padding: 4px 8px; background-color: #F8FAFC; border: 1px solid #E2E8F0; border-radius: 4px;");
    } else {
        m_customerBaqayaLabel->setText("Walk-in Customer (Credit/Khata not allowed without selecting customer account)");
        m_customerBaqayaLabel->setStyleSheet("color: #64748B; font-size: 11px; padding: 4px 8px; background-color: #F8FAFC; border: 1px solid #E2E8F0; border-radius: 4px; font-style: italic;");
    }
    mainLayout->addWidget(m_customerBaqayaLabel);

    // 4. Split Allocation Matrix
    auto* splitFrame = new QFrame(this);
    splitFrame->setStyleSheet("background-color: #F8FAFC; border: 1px solid #E2E8F0; border-radius: 6px; padding: 6px;");
    auto* grid = new QGridLayout(splitFrame);
    grid->setSpacing(8);
    grid->setContentsMargins(10, 8, 10, 8);

    // Row 0: Cash
    auto* cashLbl = new QLabel("Cash Tendered:", splitFrame);
    cashLbl->setStyleSheet("font-weight: 700; color: #1E293B;");
    m_cashTenderedInput = new MoneyInput(splitFrame);
    m_cashTenderedInput->setFixedHeight(28);
    m_cashTenderedInput->setValue(totalAmount);
    grid->addWidget(cashLbl, 0, 0);
    grid->addWidget(m_cashTenderedInput, 0, 1, 1, 2);

    // Row 1: Digital
    auto* digitalLbl = new QLabel("Digital Payment:", splitFrame);
    digitalLbl->setStyleSheet("font-weight: 700; color: #1E293B;");
    
    m_digitalProviderCombo = new QComboBox(splitFrame);
    m_digitalProviderCombo->setFixedHeight(28);
    m_digitalProviderCombo->addItem("EasyPaisa", static_cast<int>(domain::PaymentType::Easypaisa));
    m_digitalProviderCombo->addItem("JazzCash", static_cast<int>(domain::PaymentType::JazzCash));
    m_digitalProviderCombo->addItem("Card", static_cast<int>(domain::PaymentType::Card));
    m_digitalProviderCombo->addItem("Bank / Raast", static_cast<int>(domain::PaymentType::BankTransfer));

    m_digitalAmountInput = new MoneyInput(splitFrame);
    m_digitalAmountInput->setFixedHeight(28);
    m_digitalAmountInput->setValue(core::Money(0));

    m_digitalRefInput = new QLineEdit(splitFrame);
    m_digitalRefInput->setFixedHeight(28);
    m_digitalRefInput->setPlaceholderText("TID / Ref # (Optional)");

    grid->addWidget(digitalLbl, 1, 0);
    grid->addWidget(m_digitalProviderCombo, 1, 1);
    grid->addWidget(m_digitalAmountInput, 1, 2);
    grid->addWidget(m_digitalRefInput, 1, 3);

    // Row 2: Khata
    auto* khataLbl = new QLabel("To Customer Khata:", splitFrame);
    khataLbl->setStyleSheet("font-weight: 700; color: #1E293B;");
    m_khataAmountInput = new MoneyInput(splitFrame);
    m_khataAmountInput->setFixedHeight(28);
    m_khataAmountInput->setValue(core::Money(0));
    m_khataAmountInput->setEnabled(m_customer.id > 1);

    grid->addWidget(khataLbl, 2, 0);
    grid->addWidget(m_khataAmountInput, 2, 1, 1, 2);

    mainLayout->addWidget(splitFrame);

    // 5. Allocation Status & Change Bar
    auto* statusBox = new QFrame(this);
    statusBox->setStyleSheet("background-color: #FFFFFF; border: 1px solid #CBD5E1; border-radius: 6px; padding: 6px;");
    auto* statusLayout = new QGridLayout(statusBox);
    statusLayout->setSpacing(6);
    statusLayout->setContentsMargins(10, 6, 10, 6);

    auto* allocTitle = new QLabel("Allocated:", statusBox);
    allocTitle->setStyleSheet("font-size: 11px; font-weight: 700; color: #64748B;");
    m_allocatedLabel = new QLabel(totalAmount.formatted(), statusBox);
    m_allocatedLabel->setStyleSheet("font-size: 13px; font-weight: 700; color: #0F172A;");

    auto* remTitle = new QLabel("Remaining:", statusBox);
    remTitle->setStyleSheet("font-size: 11px; font-weight: 700; color: #64748B;");
    m_remainingLabel = new QLabel("Rs. 0.00", statusBox);
    m_remainingLabel->setStyleSheet("font-size: 13px; font-weight: 700; color: #16A34A;");

    auto* chgTitle = new QLabel("Cash Change:", statusBox);
    chgTitle->setStyleSheet("font-size: 11px; font-weight: 700; color: #64748B;");
    m_changeLabel = new QLabel("Rs. 0.00", statusBox);
    m_changeLabel->setStyleSheet("font-size: 15px; font-weight: 800; color: #16A34A;");

    statusLayout->addWidget(allocTitle, 0, 0);
    statusLayout->addWidget(m_allocatedLabel, 0, 1);
    statusLayout->addWidget(remTitle, 0, 2);
    statusLayout->addWidget(m_remainingLabel, 0, 3);
    statusLayout->addWidget(chgTitle, 1, 0);
    statusLayout->addWidget(m_changeLabel, 1, 1, 1, 3);

    mainLayout->addWidget(statusBox);

    // 6. Action buttons
    auto* btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(10);

    auto* cancelBtn = AppButton::secondary("Cancel (Esc)", AppButton::Size::Medium, this);
    cancelBtn->setFixedHeight(32);

    m_confirmBtn = AppButton::primary("Complete && Print (Enter)", AppButton::Size::Medium, this);
    m_confirmBtn->setObjectName("primaryCheckoutBtn");
    m_confirmBtn->setFixedHeight(32);
    m_confirmBtn->setDefault(true);

    btnLayout->addWidget(cancelBtn, 1);
    btnLayout->addWidget(m_confirmBtn, 1);
    mainLayout->addLayout(btnLayout);

    connect(m_cashTenderedInput, &MoneyInput::valueChanged, this, &PaymentDialog::recalculateSettlement);
    connect(m_digitalAmountInput, &MoneyInput::valueChanged, this, &PaymentDialog::recalculateSettlement);
    connect(m_khataAmountInput, &MoneyInput::valueChanged, this, &PaymentDialog::recalculateSettlement);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_confirmBtn, &QPushButton::clicked, this, &QDialog::accept);

    recalculateSettlement();
    m_cashTenderedInput->setFocus();
    m_cashTenderedInput->selectAll();
}

void PaymentDialog::recalculateSettlement()
{
    core::Money cash = m_cashTenderedInput->value();
    core::Money digital = m_digitalAmountInput->value();
    core::Money khata = m_khataAmountInput->value();

    core::Money nonCash = digital + khata;
    core::Money totalTendered = cash + nonCash;

    m_allocatedLabel->setText(totalTendered.formatted());

    if (totalTendered < m_totalAmount) {
        core::Money shortAmount = m_totalAmount - totalTendered;
        m_remainingLabel->setText(QString("Short: %1").arg(shortAmount.formatted()));
        m_remainingLabel->setStyleSheet("font-size: 13px; font-weight: 700; color: #DC2626;");
        m_changeLabel->setText("Rs. 0.00");
        m_changeLabel->setStyleSheet("font-size: 15px; font-weight: 800; color: #64748B;");
        m_confirmBtn->setEnabled(false);
    } else {
        m_remainingLabel->setText("✓ Fully Allocated");
        m_remainingLabel->setStyleSheet("font-size: 13px; font-weight: 700; color: #16A34A;");
        
        // Change is calculated from cash tendered
        core::Money neededFromCash = m_totalAmount - nonCash;
        if (neededFromCash.isNegative()) neededFromCash = core::Money(0);
        core::Money change = cash - neededFromCash;
        if (change.isNegative()) change = core::Money(0);

        m_changeLabel->setText(change.formatted());
        m_changeLabel->setStyleSheet("font-size: 15px; font-weight: 800; color: #16A34A;");
        m_confirmBtn->setEnabled(true);
    }

    if (m_customer.id > 1 && khata.isPositive()) {
        core::Money newBaqaya = m_customer.baqaya + khata;
        m_customerBaqayaLabel->setText(
            QString("Customer: <b>%1</b> &nbsp;|&nbsp; Current: <b>%2</b> &nbsp;➔&nbsp; <span style='color: #DC2626;'>New Baqaya: <b>%3</b></span>")
                .arg(m_customer.name, m_customer.baqaya.formatted(), newBaqaya.formatted())
        );
    }
}

void PaymentDialog::setPresetExactCash()
{
    m_cashTenderedInput->setValue(m_totalAmount);
    m_digitalAmountInput->setValue(core::Money(0));
    m_khataAmountInput->setValue(core::Money(0));
    recalculateSettlement();
    m_cashTenderedInput->setFocus();
    m_cashTenderedInput->selectAll();
}

void PaymentDialog::setPresetEasyPaisa()
{
    m_digitalProviderCombo->setCurrentIndex(0); // EasyPaisa
    m_digitalAmountInput->setValue(m_totalAmount);
    m_cashTenderedInput->setValue(core::Money(0));
    m_khataAmountInput->setValue(core::Money(0));
    recalculateSettlement();
    m_digitalRefInput->setFocus();
}

void PaymentDialog::setPresetJazzCash()
{
    m_digitalProviderCombo->setCurrentIndex(1); // JazzCash
    m_digitalAmountInput->setValue(m_totalAmount);
    m_cashTenderedInput->setValue(core::Money(0));
    m_khataAmountInput->setValue(core::Money(0));
    recalculateSettlement();
    m_digitalRefInput->setFocus();
}

void PaymentDialog::setPresetCard()
{
    m_digitalProviderCombo->setCurrentIndex(2); // Card
    m_digitalAmountInput->setValue(m_totalAmount);
    m_cashTenderedInput->setValue(core::Money(0));
    m_khataAmountInput->setValue(core::Money(0));
    recalculateSettlement();
    m_digitalRefInput->setFocus();
}

void PaymentDialog::setPresetKhata()
{
    if (m_customer.id <= 1) return;
    m_khataAmountInput->setValue(m_totalAmount);
    m_cashTenderedInput->setValue(core::Money(0));
    m_digitalAmountInput->setValue(core::Money(0));
    recalculateSettlement();
    m_confirmBtn->setFocus();
}

domain::PaymentType PaymentDialog::selectedPaymentType() const
{
    core::Money cash = m_cashTenderedInput->value();
    core::Money digital = m_digitalAmountInput->value();
    core::Money khata = m_khataAmountInput->value();

    int nonZeroCount = (cash.isPositive() ? 1 : 0) + (digital.isPositive() ? 1 : 0) + (khata.isPositive() ? 1 : 0);
    if (nonZeroCount > 1) {
        // Multi-tender split payment
        return domain::PaymentType::Cash;
    }

    if (khata.isPositive()) return domain::PaymentType::Udhaar;
    if (digital.isPositive()) return static_cast<domain::PaymentType>(m_digitalProviderCombo->currentData().toInt());
    return domain::PaymentType::Cash;
}

std::vector<domain::PaymentAllocation> PaymentDialog::paymentAllocations() const
{
    std::vector<domain::PaymentAllocation> list;
    core::Money cash = m_cashTenderedInput->value();
    core::Money digital = m_digitalAmountInput->value();
    core::Money khata = m_khataAmountInput->value();

    if (cash.isPositive()) {
        core::Money nonCash = digital + khata;
        core::Money neededCash = m_totalAmount - nonCash;
        if (neededCash.isNegative()) neededCash = core::Money(0);
        core::Money actualCashUsed = (cash < neededCash) ? cash : neededCash;
        list.push_back({domain::PaymentType::Cash, actualCashUsed, ""});
    }

    if (digital.isPositive()) {
        auto dType = static_cast<domain::PaymentType>(m_digitalProviderCombo->currentData().toInt());
        list.push_back({dType, digital, m_digitalRefInput->text().trimmed()});
    }

    if (khata.isPositive() && m_customer.id > 1) {
        list.push_back({domain::PaymentType::Udhaar, khata, "Customer Khata"});
    }

    if (list.empty()) {
        list.push_back({domain::PaymentType::Cash, m_totalAmount, ""});
    }

    return list;
}

core::Money PaymentDialog::cashReceived() const
{
    return m_cashTenderedInput->value();
}

core::Money PaymentDialog::changeGiven() const
{
    core::Money cash = m_cashTenderedInput->value();
    core::Money nonCash = m_digitalAmountInput->value() + m_khataAmountInput->value();
    core::Money neededCash = m_totalAmount - nonCash;
    if (neededCash.isNegative()) neededCash = core::Money(0);
    core::Money diff = cash - neededCash;
    return diff.isPositive() ? diff : core::Money(0);
}

int PaymentDialog::selectedCustomerId() const
{
    return m_customer.id;
}

void PaymentDialog::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_F1:
        setPresetExactCash();
        event->accept();
        return;
    case Qt::Key_F2:
        setPresetEasyPaisa();
        event->accept();
        return;
    case Qt::Key_F3:
        setPresetJazzCash();
        event->accept();
        return;
    case Qt::Key_F4:
        setPresetCard();
        event->accept();
        return;
    case Qt::Key_F5:
        if (m_customer.id > 1) {
            setPresetKhata();
            event->accept();
            return;
        }
        break;
    default:
        break;
    }
    QDialog::keyPressEvent(event);
}

} // namespace ui
