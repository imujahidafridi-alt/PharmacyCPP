#pragma once
#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>
#include "domain/Models.h"
#include "ui/components/DataTable.h"

namespace ui {

class SaleReturnDialog : public QWidget {
    Q_OBJECT
public:
    explicit SaleReturnDialog(QWidget* parent = nullptr);
    void loadBill(const QString& billNumber);

private slots:
    void handleFindBill();
    void handleProcessReturn();

private:
    QLineEdit* m_billInput;
    QPushButton* m_findBtn;
    
    QLabel* m_billDetailsLabel;
    DataTable* m_itemsTable;
    
    QLineEdit* m_reasonInput;
    QCheckBox* m_cashRefundCheck;
    QPushButton* m_returnBtn;

    domain::Sale m_loadedSale;
};

} // namespace ui
