#pragma once
#include <QWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include "ui/components/DataTable.h"
#include "ui/components/AppSearchBox.h"
#include "ui/components/AppDropdown.h"
#include "ui/components/AppButton.h"
#include "domain/Models.h"

namespace ui {

class StockWindow : public QWidget {
    Q_OBJECT
public:
    explicit StockWindow(QWidget* parent = nullptr);

    void refreshStock();

private slots:
    void handleSearchChanged(const QString& text);
    void handleCategoryChanged(int index);
    void handleCorrectStock();

private:
    AppSearchBox* m_searchInput;
    AppDropdown* m_categoryCombo;
    DataTable* m_table;
    AppButton* m_correctBtn;

    std::vector<domain::StockItemView> m_items;
};

} // namespace ui
