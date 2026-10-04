#pragma once
#include <QWidget>
#include <QComboBox>
#include <QPushButton>
#include "ui/components/DataTable.h"
#include "domain/Models.h"

namespace ui {

class ExpiryWindow : public QWidget {
    Q_OBJECT
public:
    explicit ExpiryWindow(QWidget* parent = nullptr);

    void refreshExpiryList();

private slots:
    void handleFilterChanged(int index);
    void handleStockCorrection();

private:
    QComboBox* m_filterCombo;
    DataTable* m_table;
    QPushButton* m_correctBtn;

    std::vector<domain::StockItemView> m_items;
};

} // namespace ui
