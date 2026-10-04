#pragma once
#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include "ui/components/DataTable.h"
#include "ui/components/AppSearchBox.h"
#include "ui/components/AppButton.h"
#include "domain/Models.h"

namespace ui {

class SuppliersWindow : public QWidget {
    Q_OBJECT
public:
    explicit SuppliersWindow(QWidget* parent = nullptr);

    void refreshSuppliers();

private slots:
    void handleAddSupplier();
    void handlePaySupplier();
    void handleSearchChanged(const QString& text);

private:
    AppSearchBox* m_searchInput;
    DataTable* m_table;
    AppButton* m_addBtn;
    AppButton* m_payBtn;

    std::vector<domain::Supplier> m_suppliers;
};

} // namespace ui
