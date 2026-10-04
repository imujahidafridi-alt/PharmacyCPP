#pragma once
#include <QLabel>
#include "domain/Models.h"

namespace ui {

class StatusBadge : public QLabel {
    Q_OBJECT
public:
    explicit StatusBadge(QWidget* parent = nullptr);
    void setStatus(domain::StockStatus status);
};

} // namespace ui
