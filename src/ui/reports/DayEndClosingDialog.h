#pragma once
#include "ui/components/AppModal.h"
#include "ui/components/AppTextInput.h"
#include "ui/components/AppButton.h"
#include "domain/Models.h"
#include <QLabel>
#include <QSpinBox>
#include <QFrame>

namespace ui {

class DayEndClosingDialog : public AppModal {
    Q_OBJECT
public:
    explicit DayEndClosingDialog(QWidget* parent = nullptr);

    domain::CashSession sessionResult() const { return m_session; }

private slots:
    void handleNoteCountChanged();
    void handleActualAmountChanged(const QString& val);
    void handlePreviewZReport();
    void handleConfirmClose();

private:
    void calculateExpectedCash();
    void updateDiscrepancyDisplay();

    domain::CashSession m_session;

    QLabel* m_openingFloatLabel{nullptr};
    QLabel* m_cashSalesLabel{nullptr};
    QLabel* m_khataPaymentsLabel{nullptr};
    QLabel* m_cashReturnsLabel{nullptr};
    QLabel* m_expectedCashLabel{nullptr};

    AppTextInput* m_actualCashInput{nullptr};
    QFrame* m_discrepancyCard{nullptr};
    QLabel* m_discrepancyLabel{nullptr};
    AppTextInput* m_notesInput{nullptr};

    // Note denomination counters
    QSpinBox* m_note5000{nullptr};
    QSpinBox* m_note1000{nullptr};
    QSpinBox* m_note500{nullptr};
    QSpinBox* m_note100{nullptr};
    QSpinBox* m_note50{nullptr};
    QSpinBox* m_note20{nullptr};
    QSpinBox* m_note10{nullptr};

    bool m_updatingFromNotes{false};
};

} // namespace ui
