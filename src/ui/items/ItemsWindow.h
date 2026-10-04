#pragma once
#include <QWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include "ui/components/DataTable.h"
#include "ui/components/AppSearchBox.h"
#include "ui/components/AppDropdown.h"
#include "ui/components/AppButton.h"

namespace ui {

class ItemsWindow : public QWidget {
    Q_OBJECT
public:
    explicit ItemsWindow(QWidget* parent = nullptr);

    void refreshItems();

private slots:
    void handleAddItem();
    void handleSearchChanged(const QString& text);
    void handleCategoryFilter(int index);

private:
    AppSearchBox* m_searchInput;
    AppDropdown* m_categoryCombo;
    DataTable* m_table;
    AppButton* m_addBtn;
};

} // namespace ui
