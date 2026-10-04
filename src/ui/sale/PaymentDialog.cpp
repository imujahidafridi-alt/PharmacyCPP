#include "ui/sale/PaymentDialog.h"
#include "ui/components/AppButton.h"
#include "ui/components/AppToast.h"
#include "services/LedgerService.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QKeyEvent>
#include <QInputDialog>
#include <algorithm>

namespace ui {

PaymentDialog::PaymentDialog(core::Money totalAmount, domain::Customer currentCustomer, QWidget* parent)
    : QDialog(parent), m_totalAmount(totalAmount), m_customer(currentCustomer)
{
    setWindowTitle("Settlement & Payment (F9)");
    setFixedWidth(560);
    setStyleSheet(R"(
        QDialog { background-color: #FFFFFF; }
        QLabel { font-size: 12px; }
        QPushButton.modePill {
            background-color: #F1F5F9;
            color: #334155;
            border: 1px solid #CBD5E1;
            border-radius: 5px;
            font-size: 11px;
            font-weight: 700;
            padding: 5px 8px;
        }
        QPushButton.modePill:hover {
            background-color: #E2E8F0;
            color: #0F172A;
            border-color: #94A3B8;
        }
        QPushButton.modePill:checked {
            background-color: #0F766E;
            color: #FFFFFF;
            border-color: #0F766E;
        }
        QPushButton.denomBtn {
            background-color: #F8FAFC;
            color: #0F172A;
            border: 1px solid #CBD5E1;
            border-radius: 4px;
            font-size: 11px;
            font-weight: 700;
            padding: 4px 6px;
        }
        QPushButton.denomBtn:hover {
            background-color: #E0F2FE;
            border-color: #38BDF8;
            color: #0369A1;
        }
        QPushButton.denomBtn:pressed {
            background-color: #BAE6FD;
        }
        QPushButton.denomBtnAdd {
            background-color: #F0FDF4;
            color: #166534;
            border: 1px solid #BBF7D0;
            border-radius: 4px;
            font-size: 11px;
            font-weight: 700;
            padding: 4px 6px;
        }
        QPushButton.denomBtnAdd:hover {
            background-color: #DCFCE7;
            border-color: #86EFAC;
            color: #14532D;
        }
    )");

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(18, 16, 18, 16);

    // 1. Total Display Card & Customer Context
    auto* topCard = new QFrame(this);
    topCard->setObjectName("paymentTopCard");
    topCard->setStyleSheet("#paymentTopCard { background-color: #F0FDFA; border: 1.5px solid #0F766E; border-radius: 6px; }");
    auto* topLayout = new QVBoxLayout(topCard);
    topLayout->setSpacing(4);
    topLayout->setContentsMargins(14, 10, 14, 10);

    auto* totalHeaderRow = new QHBoxLayout();
    auto* totalTitle = new QLabel("TOTAL PAYABLE (NET)", topCard);
    totalTitle->setStyleSheet("color: #0F766E; font-weight: 700; font-size: 11px; letter-spacing: 0.5px; background: transparent;");
    totalHeaderRow->addWidget(totalTitle);
    totalHeaderRow->addStretch();

    // Customer name + Change button right on header
    auto* custRow = new QHBoxLayout();
    m_customerInfoLabel = new QLabel(topCard);
    m_customerInfoLabel->setStyleSheet("font-size: 11px; color: #334155; font-weight: 600; background: transparent;");

    m_changeCustomerBtn = new QPushButton("Change (F8)", topCard);
    m_changeCustomerBtn->setFixedHeight(22);
    m_changeCustomerBtn->setCursor(Qt::PointingHandCursor);
    m_changeCustomerBtn->setStyleSheet(
        "QPushButton { background-color: #FFFFFF; color: #0F766E; border: 1px solid #0F766E; border-radius: 3px; font-size: 10px; font-weight: 700; padding: 2px 6px; }"
        "QPushButton:hover { background-color: #CCFBF1; }"
    );
    connect(m_changeCustomerBtn, &QPushButton::clicked, this, &PaymentDialog::openCustomerSelection);

    custRow->addWidget(m_customerInfoLabel);
    custRow->addSpacing(6);
    custRow->addWidget(m_changeCustomerBtn);
    totalHeaderRow->addLayout(custRow);

    m_totalLabel = new QLabel(totalAmount.formatted(), topCard);
    m_totalLabel->setStyleSheet("color: #0F766E; font-size: 26px; font-weight: 800; background: transparent;");

    topLayout->addLayout(totalHeaderRow);
    topLayout->addWidget(m_totalLabel);
    mainLayout->addWidget(topCard);

    // 2. Mode Pills (Cash, EasyPaisa, JazzCash, Card, Khata, Split)
    m_modeGroup = new QButtonGroup(this);
    m_modeGroup->setExclusive(true);

    auto* modeLayout = new QHBoxLayout();
    modeLayout->setSpacing(6);

    auto makeModeBtn = [this, modeLayout](const QString& text, int id) {
        auto* btn = new QPushButton(text, this);
        btn->setProperty("class", "modePill");
        btn->setCheckable(true);
        btn->setFixedHeight(30);
        btn->setCursor(Qt::PointingHandCursor);
        m_modeGroup->addButton(btn, id);
        modeLayout->addWidget(btn);
        return btn;
    };

    m_btnCash = makeModeBtn("💵 Cash (F1)", 0);
    m_btnEasyPaisa = makeModeBtn("📱 EasyPaisa (F2)", 1);
    m_btnJazzCash = makeModeBtn("📲 JazzCash (F3)", 2);
    m_btnCard = makeModeBtn("💳 Card (F4)", 3);
    m_btnKhata = makeModeBtn("📝 Khata (F5)", 4);
    m_btnSplit = makeModeBtn("🔀 Split (F6)", 5);

    m_btnCash->setChecked(true);
    mainLayout->addLayout(modeLayout);

    connect(m_btnCash, &QPushButton::clicked, this, &PaymentDialog::setPresetCash);
    connect(m_btnEasyPaisa, &QPushButton::clicked, this, &PaymentDialog::setPresetEasyPaisa);
    connect(m_btnJazzCash, &QPushButton::clicked, this, &PaymentDialog::setPresetJazzCash);
    connect(m_btnCard, &QPushButton::clicked, this, &PaymentDialog::setPresetCard);
    connect(m_btnKhata, &QPushButton::clicked, this, &PaymentDialog::setPresetKhata);
    connect(m_btnSplit, &QPushButton::clicked, this, &PaymentDialog::setPresetSplit);

    // 3. Quick Cash Notes / Denomination Card (Pakistan Currency)
    m_denominationCard = new QWidget(this);
    m_denominationCard->setStyleSheet("background-color: #F8FAFC; border: 1px solid #E2E8F0; border-radius: 6px;");
    auto* denomLayout = new QVBoxLayout(m_denominationCard);
    denomLayout->setContentsMargins(10, 8, 10, 8);
    denomLayout->setSpacing(6);

    auto* denomTitle = new QLabel("QUICK CASH NOTES / CHIPS", m_denominationCard);
    denomTitle->setStyleSheet("font-size: 10px; font-weight: 800; color: #64748B; letter-spacing: 0.5px; border: none; background: transparent;");
    denomLayout->addWidget(denomTitle);

    // Row A: Exact & Increments (+100, +500, +1000, Clear)
    auto* denomRowA = new QHBoxLayout();
    denomRowA->setSpacing(6);

    auto makeDenomBtn = [this](const QString& text, const QString& extraClass, auto onClick) {
        auto* btn = new QPushButton(text, this);
        btn->setProperty("class", extraClass.isEmpty() ? "denomBtn" : extraClass);
        btn->setFixedHeight(28);
        btn->setCursor(Qt::PointingHandCursor);
        connect(btn, &QPushButton::clicked, this, onClick);
        return btn;
    };

    auto* btnExact = makeDenomBtn(QString("💵 Exact (%1)").arg(totalAmount.formatted()), "", [this]() {
        m_cashTenderedInput->setValue(m_totalAmount);
        recalculateSettlement();
        m_confirmBtn->setFocus();
    });
    auto* btnPlus100 = makeDenomBtn("+100", "denomBtnAdd", [this]() { addCash(100); });
    auto* btnPlus500 = makeDenomBtn("+500", "denomBtnAdd", [this]() { addCash(500); });
    auto* btnPlus1000 = makeDenomBtn("+1,000", "denomBtnAdd", [this]() { addCash(1000); });
    auto* btnClear = makeDenomBtn("Clear (C)", "", [this]() { clearCash(); });

    denomRowA->addWidget(btnExact, 2);
    denomRowA->addWidget(btnPlus100, 1);
    denomRowA->addWidget(btnPlus500, 1);
    denomRowA->addWidget(btnPlus1000, 1);
    denomRowA->addWidget(btnClear, 1);
    denomLayout->addLayout(denomRowA);

    // Row B: Standard Pakistani Notes (Rs. 500, Rs. 1000, Rs. 5000, Dynamic Next Note)
    auto* denomRowB = new QHBoxLayout();
    denomRowB->setSpacing(6);

    auto* btn500 = makeDenomBtn("Rs. 500", "", [this]() { setCash(500); });
    auto* btn1000 = makeDenomBtn("Rs. 1,000", "", [this]() { setCash(1000); });
    auto* btn5000 = makeDenomBtn("Rs. 5,000", "", [this]() { setCash(5000); });

    // Dynamic Next Round Note button (e.g. if bill is 340 -> 500, if 1250 -> 1500 / 2000)
    int64_t totalRupees = (m_totalAmount.paisa() + 99) / 100;
    int nextRoundAmount = 0;
    if (totalRupees < 500) nextRoundAmount = 500;
    else if (totalRupees < 1000) nextRoundAmount = 1000;
    else if (totalRupees < 2000) nextRoundAmount = static_cast<int>(((totalRupees + 499) / 500) * 500);
    else if (totalRupees < 5000) nextRoundAmount = static_cast<int>(((totalRupees + 999) / 1000) * 1000);
    else nextRoundAmount = static_cast<int>(((totalRupees + 4999) / 5000) * 5000);

    auto* btnNextRound = makeDenomBtn(QString("Round: Rs. %1").arg(nextRoundAmount), "", [this, nextRoundAmount]() {
        setCash(nextRoundAmount);
    });

    denomRowB->addWidget(btn500, 1);
    denomRowB->addWidget(btn1000, 1);
    denomRowB->addWidget(btn5000, 1);
    denomRowB->addWidget(btnNextRound, 2);
    denomLayout->addLayout(denomRowB);

    mainLayout->addWidget(m_denominationCard);

    // 4. Input Fields Container
    auto* inputContainer = new QFrame(this);
    inputContainer->setStyleSheet("background-color: #F8FAFC; border: 1px solid #E2E8F0; border-radius: 6px;");
    auto* inputLayout = new QVBoxLayout(inputContainer);
    inputLayout->setContentsMargins(10, 8, 10, 8);
    inputLayout->setSpacing(8);

    // Cash Row
    m_cashRow = new QWidget(inputContainer);
    auto* cashLayout = new QHBoxLayout(m_cashRow);
    cashLayout->setContentsMargins(0, 0, 0, 0);
    cashLayout->setSpacing(8);

    auto* cashLbl = new QLabel("Cash Tendered:", m_cashRow);
    cashLbl->setFixedWidth(115);
    cashLbl->setStyleSheet("font-weight: 700; color: #1E293B; border: none; background: transparent;");

    m_cashTenderedInput = new MoneyInput(m_cashRow);
    m_cashTenderedInput->setFixedHeight(32);
    m_cashTenderedInput->setStyleSheet("font-size: 15px; font-weight: 800; color: #0F172A; padding: 2px 6px;");
    m_cashTenderedInput->setValue(totalAmount);

    cashLayout->addWidget(cashLbl);
    cashLayout->addWidget(m_cashTenderedInput);
    inputLayout->addWidget(m_cashRow);

    // Digital Row
    m_digitalRow = new QWidget(inputContainer);
    auto* digitalLayout = new QHBoxLayout(m_digitalRow);
    digitalLayout->setContentsMargins(0, 0, 0, 0);
    digitalLayout->setSpacing(8);

    auto* digitalLbl = new QLabel("Digital Payment:", m_digitalRow);
    digitalLbl->setFixedWidth(115);
    digitalLbl->setStyleSheet("font-weight: 700; color: #1E293B; border: none; background: transparent;");

    m_digitalProviderCombo = new QComboBox(m_digitalRow);
    m_digitalProviderCombo->setFixedHeight(30);
    m_digitalProviderCombo->setFixedWidth(130);
    m_digitalProviderCombo->addItem("EasyPaisa", static_cast<int>(domain::PaymentType::Easypaisa));
    m_digitalProviderCombo->addItem("JazzCash", static_cast<int>(domain::PaymentType::JazzCash));
    m_digitalProviderCombo->addItem("Card", static_cast<int>(domain::PaymentType::Card));
    m_digitalProviderCombo->addItem("Bank / Raast", static_cast<int>(domain::PaymentType::BankTransfer));

    m_digitalAmountInput = new MoneyInput(m_digitalRow);
    m_digitalAmountInput->setFixedHeight(30);
    m_digitalAmountInput->setValue(core::Money(0));

    m_digitalRefInput = new QLineEdit(m_digitalRow);
    m_digitalRefInput->setFixedHeight(30);
    m_digitalRefInput->setPlaceholderText("TID / Ref # (Optional)");

    digitalLayout->addWidget(digitalLbl);
    digitalLayout->addWidget(m_digitalProviderCombo);
    digitalLayout->addWidget(m_digitalAmountInput);
    digitalLayout->addWidget(m_digitalRefInput);
    inputLayout->addWidget(m_digitalRow);

    // Khata Row
    m_khataRow = new QWidget(inputContainer);
    auto* khataLayout = new QVBoxLayout(m_khataRow);
    khataLayout->setContentsMargins(0, 0, 0, 0);
    khataLayout->setSpacing(4);

    auto* khataInputRow = new QHBoxLayout();
    khataInputRow->setSpacing(8);

    auto* khataLbl = new QLabel("To Khata (Udhaar):", m_khataRow);
    khataLbl->setFixedWidth(115);
    khataLbl->setStyleSheet("font-weight: 700; color: #1E293B; border: none; background: transparent;");

    m_khataAmountInput = new MoneyInput(m_khataRow);
    m_khataAmountInput->setFixedHeight(30);
    m_khataAmountInput->setValue(core::Money(0));

    khataInputRow->addWidget(khataLbl);
    khataInputRow->addWidget(m_khataAmountInput);
    khataLayout->addLayout(khataInputRow);

    m_khataPreviewLabel = new QLabel(m_khataRow);
    m_khataPreviewLabel->setStyleSheet("font-size: 11px; color: #64748B; border: none; background: transparent; padding-left: 123px;");
    khataLayout->addWidget(m_khataPreviewLabel);

    inputLayout->addWidget(m_khataRow);
    mainLayout->addWidget(inputContainer);

    // 5. Settlement Status & Change Breakdown Card
    auto* statusBox = new QFrame(this);
    statusBox->setStyleSheet("background-color: #FFFFFF; border: 1.5px solid #CBD5E1; border-radius: 6px;");
    auto* statusLayout = new QVBoxLayout(statusBox);
    statusLayout->setSpacing(6);
    statusLayout->setContentsMargins(12, 8, 12, 8);

    auto* statusGrid = new QGridLayout();
    statusGrid->setSpacing(6);

    auto* allocTitle = new QLabel("Total Tendered:", statusBox);
    allocTitle->setStyleSheet("font-size: 11px; font-weight: 700; color: #64748B; border: none; background: transparent;");
    m_allocatedLabel = new QLabel(totalAmount.formatted(), statusBox);
    m_allocatedLabel->setStyleSheet("font-size: 14px; font-weight: 700; color: #0F172A; border: none; background: transparent;");

    auto* statusTitle = new QLabel("Settlement Status:", statusBox);
    statusTitle->setStyleSheet("font-size: 11px; font-weight: 700; color: #64748B; border: none; background: transparent;");
    m_statusLabel = new QLabel("✓ Fully Paid", statusBox);
    m_statusLabel->setStyleSheet("font-size: 14px; font-weight: 700; color: #16A34A; border: none; background: transparent;");

    auto* chgTitle = new QLabel("Change to Return:", statusBox);
    chgTitle->setStyleSheet("font-size: 11px; font-weight: 700; color: #64748B; border: none; background: transparent;");
    m_changeLabel = new QLabel("Rs. 0.00", statusBox);
    m_changeLabel->setStyleSheet("font-size: 18px; font-weight: 800; color: #16A34A; border: none; background: transparent;");

    statusGrid->addWidget(allocTitle, 0, 0);
    statusGrid->addWidget(m_allocatedLabel, 0, 1);
    statusGrid->addWidget(statusTitle, 0, 2);
    statusGrid->addWidget(m_statusLabel, 0, 3);
    statusGrid->addWidget(chgTitle, 1, 0);
    statusGrid->addWidget(m_changeLabel, 1, 1, 1, 3);
    statusLayout->addLayout(statusGrid);

    // Change note breakdown (e.g. 1x Rs. 500, 1x Rs. 50)
    m_breakdownLabel = new QLabel(statusBox);
    m_breakdownLabel->setStyleSheet("font-size: 11px; font-weight: 700; color: #0D9488; background-color: #F0FDFA; border: 1px solid #CCFBF1; border-radius: 4px; padding: 4px 8px;");
    m_breakdownLabel->setVisible(false);
    statusLayout->addWidget(m_breakdownLabel);

    mainLayout->addWidget(statusBox);

    // 6. Action buttons
    auto* btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(10);

    auto* cancelBtn = AppButton::secondary("Cancel (Esc)", AppButton::Size::Medium, this);
    cancelBtn->setFixedHeight(36);

    m_confirmBtn = AppButton::primary("Complete && Print (Enter)", AppButton::Size::Medium, this);
    m_confirmBtn->setObjectName("primaryCheckoutBtn");
    m_confirmBtn->setFixedHeight(36);
    m_confirmBtn->setDefault(true);

    btnLayout->addWidget(cancelBtn, 1);
    btnLayout->addWidget(m_confirmBtn, 2);
    mainLayout->addLayout(btnLayout);

    connect(m_cashTenderedInput, &MoneyInput::valueChanged, this, &PaymentDialog::recalculateSettlement);
    connect(m_digitalAmountInput, &MoneyInput::valueChanged, this, &PaymentDialog::recalculateSettlement);
    connect(m_khataAmountInput, &MoneyInput::valueChanged, this, &PaymentDialog::recalculateSettlement);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_confirmBtn, &QPushButton::clicked, this, &QDialog::accept);

    // Initialize display state
    setPresetCash();
}

void PaymentDialog::updateModePills(int activeIndex)
{
    m_btnCash->setChecked(activeIndex == 0);
    m_btnEasyPaisa->setChecked(activeIndex == 1);
    m_btnJazzCash->setChecked(activeIndex == 2);
    m_btnCard->setChecked(activeIndex == 3);
    m_btnKhata->setChecked(activeIndex == 4);
    m_btnSplit->setChecked(activeIndex == 5);

    // Show/hide sections based on mode
    bool isSplit = (activeIndex == 5);
    bool isCash = (activeIndex == 0 || isSplit);
    bool isDigital = (activeIndex >= 1 && activeIndex <= 3) || isSplit;
    bool isKhata = (activeIndex == 4 || isSplit);

    m_denominationCard->setVisible(isCash);
    m_cashRow->setVisible(isCash);
    m_digitalRow->setVisible(isDigital);
    m_khataRow->setVisible(isKhata);
}

void PaymentDialog::setPresetCash()
{
    updateModePills(0);
    m_cashTenderedInput->setValue(m_totalAmount);
    m_digitalAmountInput->setValue(core::Money(0));
    m_khataAmountInput->setValue(core::Money(0));
    recalculateSettlement();
    m_cashTenderedInput->setFocus();
    m_cashTenderedInput->selectAll();
}

void PaymentDialog::setPresetEasyPaisa()
{
    updateModePills(1);
    m_digitalProviderCombo->setCurrentIndex(0);
    m_digitalAmountInput->setValue(m_totalAmount);
    m_cashTenderedInput->setValue(core::Money(0));
    m_khataAmountInput->setValue(core::Money(0));
    recalculateSettlement();
    m_digitalRefInput->setFocus();
}

void PaymentDialog::setPresetJazzCash()
{
    updateModePills(2);
    m_digitalProviderCombo->setCurrentIndex(1);
    m_digitalAmountInput->setValue(m_totalAmount);
    m_cashTenderedInput->setValue(core::Money(0));
    m_khataAmountInput->setValue(core::Money(0));
    recalculateSettlement();
    m_digitalRefInput->setFocus();
}

void PaymentDialog::setPresetCard()
{
    updateModePills(3);
    m_digitalProviderCombo->setCurrentIndex(2);
    m_digitalAmountInput->setValue(m_totalAmount);
    m_cashTenderedInput->setValue(core::Money(0));
    m_khataAmountInput->setValue(core::Money(0));
    recalculateSettlement();
    m_digitalRefInput->setFocus();
}

void PaymentDialog::setPresetKhata()
{
    if (m_customer.id <= 1) {
        // Prompt user to pick customer before enabling Khata
        openCustomerSelection();
        if (m_customer.id <= 1) {
            // User cancelled customer selection, revert to cash
            setPresetCash();
            return;
        }
    }

    updateModePills(4);
    m_khataAmountInput->setValue(m_totalAmount);
    m_cashTenderedInput->setValue(core::Money(0));
    m_digitalAmountInput->setValue(core::Money(0));
    recalculateSettlement();
    m_confirmBtn->setFocus();
}

void PaymentDialog::setPresetSplit()
{
    updateModePills(5);
    recalculateSettlement();
    m_cashTenderedInput->setFocus();
    m_cashTenderedInput->selectAll();
}

void PaymentDialog::addCash(int amountRupees)
{
    core::Money current = m_cashTenderedInput->value();
    m_cashTenderedInput->setValue(current + core::Money::fromRupees(amountRupees));
    recalculateSettlement();
    m_cashTenderedInput->setFocus();
    m_cashTenderedInput->selectAll();
}

void PaymentDialog::setCash(int amountRupees)
{
    m_cashTenderedInput->setValue(core::Money::fromRupees(amountRupees));
    recalculateSettlement();
    m_cashTenderedInput->setFocus();
    m_cashTenderedInput->selectAll();
}

void PaymentDialog::clearCash()
{
    m_cashTenderedInput->setValue(core::Money(0));
    recalculateSettlement();
    m_cashTenderedInput->setFocus();
}

void PaymentDialog::openCustomerSelection()
{
    auto custRes = services::LedgerService::instance().searchCustomers("");
    if (custRes.isErr()) {
        AppToast::showWarning(this, "Could not load customers: " + custRes.error().userMessage());
        return;
    }

    QStringList options;
    options << "[+] Quick Add New Customer...";
    for (const auto& c : custRes.value()) {
        options << QString("%1 (%2) - Baqaya: %3").arg(c.name, c.phone.isEmpty() ? "Walk-in" : c.phone, c.baqaya.formatted());
    }

    bool ok = false;
    QString chosen = QInputDialog::getItem(this, "Select Customer Account (F8)", "Choose customer for Khata / Receipt:", options, (m_customer.id > 1 ? 1 : 0), false, &ok);
    if (!ok) return;

    if (chosen == "[+] Quick Add New Customer...") {
        QString newName = QInputDialog::getText(this, "New Customer", "Customer Full Name:", QLineEdit::Normal, "", &ok);
        if (!ok || newName.trimmed().isEmpty()) return;
        QString newPhone = QInputDialog::getText(this, "New Customer", "Mobile Phone (e.g. 03001234567):", QLineEdit::Normal, "", &ok);

        auto createRes = services::LedgerService::instance().createCustomer(newName.trimmed(), newPhone.trimmed(), "");
        if (createRes.isOk()) {
            m_customer = createRes.value();
            AppToast::showSuccess(this, QString("Customer '%1' created.").arg(m_customer.name));
        } else {
            AppToast::showWarning(this, createRes.error().userMessage());
            return;
        }
    } else {
        int idx = options.indexOf(chosen) - 1; // offset by 1 because of [+]
        if (idx >= 0 && idx < static_cast<int>(custRes.value().size())) {
            m_customer = custRes.value()[idx];
        }
    }

    recalculateSettlement();
}

QString PaymentDialog::computeChangeBreakdown(core::Money change) const
{
    if (!change.isPositive()) return "";

    int64_t rupees = change.paisa() / 100;
    if (rupees <= 0) return "";

    static const int denominations[] = {5000, 1000, 500, 100, 50, 20, 10, 5, 2, 1};
    QStringList parts;

    for (int denom : denominations) {
        if (rupees >= denom) {
            int count = static_cast<int>(rupees / denom);
            rupees %= denom;
            parts << QString("%1x Rs. %2").arg(count).arg(denom);
        }
    }

    if (parts.isEmpty()) return "";
    return QString("Give: ") + parts.join(", ");
}

void PaymentDialog::recalculateSettlement()
{
    // Update Customer strip
    if (m_customer.id > 1) {
        m_customerInfoLabel->setText(QString("Customer: <b>%1</b> (Baqaya: <b>%2</b>)").arg(m_customer.name, m_customer.baqaya.formatted()));
    } else {
        m_customerInfoLabel->setText("Customer: <i>Walk-in Customer</i>");
    }

    core::Money cash = m_cashTenderedInput->value();
    core::Money digital = m_digitalAmountInput->value();
    core::Money khata = m_khataAmountInput->value();

    core::Money nonCash = digital + khata;
    core::Money totalTendered = cash + nonCash;

    m_allocatedLabel->setText(totalTendered.formatted());

    if (totalTendered < m_totalAmount) {
        core::Money shortAmount = m_totalAmount - totalTendered;
        m_statusLabel->setText(QString("⚠ Short: %1").arg(shortAmount.formatted()));
        m_statusLabel->setStyleSheet("font-size: 14px; font-weight: 700; color: #DC2626; border: none; background: transparent;");
        m_changeLabel->setText("Rs. 0.00");
        m_changeLabel->setStyleSheet("font-size: 18px; font-weight: 800; color: #94A3B8; border: none; background: transparent;");
        m_breakdownLabel->setVisible(false);
        m_confirmBtn->setEnabled(false);
    } else {
        m_statusLabel->setText("✓ Fully Paid");
        m_statusLabel->setStyleSheet("font-size: 14px; font-weight: 700; color: #16A34A; border: none; background: transparent;");

        // Change calculation
        core::Money neededFromCash = m_totalAmount - nonCash;
        if (neededFromCash.isNegative()) neededFromCash = core::Money(0);
        core::Money change = cash - neededFromCash;
        if (change.isNegative()) change = core::Money(0);

        m_changeLabel->setText(change.formatted());
        m_changeLabel->setStyleSheet("font-size: 18px; font-weight: 800; color: #16A34A; border: none; background: transparent;");

        QString breakdown = computeChangeBreakdown(change);
        if (!breakdown.isEmpty()) {
            m_breakdownLabel->setText("💵 " + breakdown);
            m_breakdownLabel->setVisible(true);
        } else {
            m_breakdownLabel->setVisible(false);
        }

        m_confirmBtn->setEnabled(true);
    }

    // Khata preview
    if (m_customer.id > 1 && khata.isPositive()) {
        core::Money newBaqaya = m_customer.baqaya + khata;
        m_khataPreviewLabel->setText(
            QString("Current Baqaya: <b>%1</b> ➔ <span style='color: #DC2626;'>New Baqaya: <b>%2</b></span>")
                .arg(m_customer.baqaya.formatted(), newBaqaya.formatted())
        );
    } else {
        m_khataPreviewLabel->setText(m_customer.id > 1 ? "No balance being added to Khata" : "Khata disabled for Walk-in Customer");
    }
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
        setPresetCash();
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
        setPresetKhata();
        event->accept();
        return;
    case Qt::Key_F6:
        setPresetSplit();
        event->accept();
        return;
    case Qt::Key_F8:
        openCustomerSelection();
        event->accept();
        return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (m_confirmBtn->isEnabled()) {
            accept();
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
