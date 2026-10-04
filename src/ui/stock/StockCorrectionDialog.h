#pragma once
#include "ui/components/AppModal.h"
#include "ui/components/AppDropdown.h"
#include "ui/components/AppTextInput.h"
#include <QLabel>
#include <QSpinBox>

namespace ui {

class StockCorrectionDialog : public AppModal {
    Q_OBJECT
public:
    explicit StockCorrectionDialog(int itemId, const QString& itemName, int currentStock, int batchId, QWidget* parent = nullptr);

    int physicalCount() const;
    QString reason() const;

private slots:
    void handleSave();

private:
    int m_itemId;
    int m_currentStock;
    int m_batchId;

    QSpinBox* m_countSpin;
    AppDropdown* m_reasonCombo;
    AppTextInput* m_customReasonEdit;
    QLabel* m_diffLabel;
};

} // namespace ui
