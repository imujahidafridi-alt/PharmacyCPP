#pragma once
#include <QTableWidget>

namespace ui {

class DataTable : public QTableWidget {
    Q_OBJECT
public:
    explicit DataTable(QWidget* parent = nullptr);

    void setupHeaders(const QStringList& headers);
};

} // namespace ui
