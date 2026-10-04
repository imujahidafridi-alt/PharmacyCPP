#pragma once
#include <QWidget>
#include <QPushButton>
#include "ui/components/DataTable.h"

namespace ui {

class PurchasesWindow : public QWidget {
    Q_OBJECT
public:
    explicit PurchasesWindow(QWidget* parent = nullptr);

    void refreshPurchases();

private slots:
    void handleNewCashPurchase();

private:
    DataTable* m_table;
    QPushButton* m_newCashPurchaseBtn;
};

} // namespace ui
