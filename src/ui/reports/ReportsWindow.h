#pragma once
#include <QWidget>
#include <QDateEdit>
#include <QPushButton>
#include <QLabel>
#include "ui/components/DataTable.h"

namespace ui {

class ReportsWindow : public QWidget {
    Q_OBJECT
public:
    explicit ReportsWindow(QWidget* parent = nullptr);

    void refreshReports();

private slots:
    void handleGenerate();
    void handleCloseDay();
    void handleBillDoubleClicked(int row, int col);
    void handleSearchChanged(const QString& query);

private:
    QDateEdit* m_fromDateEdit;
    QDateEdit* m_toDateEdit;
    QPushButton* m_generateBtn;
    QPushButton* m_closeDayBtn;

    QLabel* m_totalSalesLabel;
    QLabel* m_cashSalesLabel;
    QLabel* m_creditSalesLabel;
    QLabel* m_profitLabel;
    QLabel* m_billCountLabel;

    class AppSearchBox* m_searchBox;
    DataTable* m_salesTable;
};

} // namespace ui
