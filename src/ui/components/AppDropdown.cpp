#include "ui/components/AppDropdown.h"
#include <QListView>
#include <QStyle>

namespace ui {

AppDropdown::AppDropdown(Size size, QWidget* parent)
    : QComboBox(parent), m_size(size)
{
    setCursor(Qt::PointingHandCursor);
    applySize();

    // Style the dropdown popup view consistently
    auto* listView = new QListView(this);
    listView->setObjectName("appDropdownPopup");
    listView->setUniformItemSizes(true);
    setView(listView);
}

void AppDropdown::setDropdownSize(Size size)
{
    m_size = size;
    applySize();
}

void AppDropdown::applySize()
{
    int h = 28;
    switch (m_size) {
        case Size::Small:
            h = 24;
            break;
        case Size::Medium:
            h = 28;
            break;
        case Size::Large:
            h = 34;
            break;
    }
    setFixedHeight(h);
}

void AppDropdown::setError(bool hasError)
{
    m_hasError = hasError;
    setProperty("hasError", hasError);
    style()->unpolish(this);
    style()->polish(this);
    update();
}

void AppDropdown::addItemWithData(const QString& text, const QVariant& userData)
{
    addItem(text, userData);
}

bool AppDropdown::selectByData(const QVariant& data)
{
    int idx = findData(data);
    if (idx != -1) {
        setCurrentIndex(idx);
        return true;
    }
    return false;
}

} // namespace ui
