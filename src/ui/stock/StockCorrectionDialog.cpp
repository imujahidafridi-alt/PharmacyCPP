#include "ui/stock/StockCorrectionDialog.h"
#include "services/StockService.h"
#include "app/AppContext.h"
#include <QFormLayout>
#include <QMessageBox>

namespace ui {

StockCorrectionDialog::StockCorrectionDialog(int itemId, const QString& itemName, int currentStock, int batchId, QWidget* parent)
    : AppModal("Stock Correction", parent), m_itemId(itemId), m_currentStock(currentStock), m_batchId(batchId)
{
    setFixedWidth(420);
    setHeader("Stock Correction", QString("Audit physical count for: %1").arg(itemName), "⚖️");
    setConfirmButton("Save Correction (Enter)", AppButton::Variant::Primary);

    auto* formLayout = new QFormLayout();
    formLayout->setSpacing(10);

    auto* sysStockLabel = new QLabel(QString("<b>%1</b> units").arg(currentStock), this);
    sysStockLabel->setStyleSheet("font-size: 13px; color: #475569;");
    formLayout->addRow("System Stock:", sysStockLabel);

    m_countSpin = new QSpinBox(this);
    m_countSpin->setRange(0, 99999);
    m_countSpin->setValue(currentStock);
    m_countSpin->setFixedHeight(28);
    formLayout->addRow("Physical Count *:", m_countSpin);

    m_diffLabel = new QLabel("Difference: 0", this);
    m_diffLabel->setStyleSheet("font-size: 12px; font-weight: 600; color: #64748B;");
    formLayout->addRow("", m_diffLabel);

    m_reasonCombo = new AppDropdown(AppDropdown::Size::Medium, this);
    m_reasonCombo->addItem("Counting Error");
    m_reasonCombo->addItem("Damaged Goods");
    m_reasonCombo->addItem("Expired Items Removed");
    m_reasonCombo->addItem("Missing / Stolen Stock");
    m_reasonCombo->addItem("Supplier Shortage");
    m_reasonCombo->addItem("Other Reason");
    formLayout->addRow("Reason *:", m_reasonCombo);

    m_customReasonEdit = new AppTextInput("Explain reason...", AppTextInput::Size::Medium, this);
    m_customReasonEdit->setVisible(false);
    formLayout->addRow("", m_customReasonEdit);

    contentLayout()->addLayout(formLayout);

    connect(m_countSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        int diff = val - m_currentStock;
        m_diffLabel->setText(QString("Difference: %1%2").arg(diff > 0 ? "+" : "", QString::number(diff)));
        m_diffLabel->setStyleSheet(diff < 0 ? "color: #DC2626; font-weight: 700;" : "color: #16A34A; font-weight: 700;");
    });

    connect(m_reasonCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        m_customReasonEdit->setVisible(idx == 5);
        if (idx == 5) m_customReasonEdit->setFocus();
    });

    disconnect(confirmButton(), &QPushButton::clicked, this, &QDialog::accept);
    connect(confirmButton(), &QPushButton::clicked, this, &StockCorrectionDialog::handleSave);
}

int StockCorrectionDialog::physicalCount() const
{
    return m_countSpin->value();
}

QString StockCorrectionDialog::reason() const
{
    if (m_reasonCombo->currentIndex() == 5) {
        return m_customReasonEdit->text().trimmed();
    }
    return m_reasonCombo->currentText();
}

void StockCorrectionDialog::handleSave()
{
    QString r = reason();
    if (r.isEmpty()) {
        m_customReasonEdit->setError(true, "Please provide a valid reason.");
        QMessageBox::warning(this, "Reason Required", "Please provide a valid reason for stock correction.");
        return;
    }
    m_customReasonEdit->setError(false);

    int userId = app::AppContext::instance().currentUser().id;
    auto res = services::StockService::instance().correctStock(m_itemId, m_batchId, physicalCount(), r, userId);
    if (res.isErr()) {
        QMessageBox::critical(this, "Correction Error", res.error().userMessage());
        return;
    }

    accept();
}

} // namespace ui
