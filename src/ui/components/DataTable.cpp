#include "ui/components/DataTable.h"
#include <QHeaderView>

namespace ui {

DataTable::DataTable(QWidget* parent) : QTableWidget(parent)
{
    setAlternatingRowColors(true);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setShowGrid(true);
    setGridStyle(Qt::SolidLine);
    setWordWrap(false);
    setCornerButtonEnabled(false);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    setFocusPolicy(Qt::StrongFocus);

    // Compact row height for enterprise density
    verticalHeader()->setVisible(false);
    verticalHeader()->setDefaultSectionSize(26);

    horizontalHeader()->setStretchLastSection(true);
    horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    horizontalHeader()->setMinimumHeight(28);
    horizontalHeader()->setHighlightSections(false);
}

void DataTable::setupHeaders(const QStringList& headers)
{
    setColumnCount(headers.size());
    setHorizontalHeaderLabels(headers);
}

} // namespace ui
