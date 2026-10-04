#include "ui/components/StatusBadge.h"

namespace ui {

StatusBadge::StatusBadge(QWidget* parent) : QLabel(parent)
{
    setAlignment(Qt::AlignCenter);
    setFixedHeight(24);
    setContentsMargins(8, 2, 8, 2);
}

void StatusBadge::setStatus(domain::StockStatus status)
{
    QString text;
    QString bgColor;
    QString fgColor;

    switch (status) {
    case domain::StockStatus::Available:
        text = "Available";
        bgColor = "#DCFCE7";
        fgColor = "#15803D";
        break;
    case domain::StockStatus::LowStock:
        text = "Low Stock";
        bgColor = "#FEF3C7";
        fgColor = "#B45309";
        break;
    case domain::StockStatus::OutOfStock:
        text = "Out of Stock";
        bgColor = "#F1F5F9";
        fgColor = "#64748B";
        break;
    case domain::StockStatus::NearExpiry:
        text = "Near Expiry";
        bgColor = "#FFEDD5";
        fgColor = "#C2410C";
        break;
    case domain::StockStatus::Expired:
        text = "Expired";
        bgColor = "#FEE2E2";
        fgColor = "#B91C1C";
        break;
    }

    setText(text);
    setStyleSheet(QString(R"(
        QLabel {
            background-color: %1;
            color: %2;
            border-radius: 12px;
            font-size: 11px;
            font-weight: 600;
            padding: 2px 8px;
        }
    )").arg(bgColor, fgColor));
}

} // namespace ui
