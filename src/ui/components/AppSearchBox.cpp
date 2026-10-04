#include "ui/components/AppSearchBox.h"
#include <QKeyEvent>

namespace ui {

AppSearchBox::AppSearchBox(const QString& placeholder, Size size, QWidget* parent)
    : QLineEdit(parent), m_size(size)
{
    setObjectName("appSearchBox");
    setPlaceholderText(placeholder);
    setClearButtonEnabled(true);
    applySize();

    m_debounceTimer = new QTimer(this);
    m_debounceTimer->setSingleShot(true);

    connect(this, &QLineEdit::textChanged, this, [this](const QString& text) {
        m_debounceTimer->stop();
        m_debounceTimer->start(m_debounceMs);
    });

    connect(m_debounceTimer, &QTimer::timeout, this, [this]() {
        emit searchDebounced(text().trimmed());
    });
}

void AppSearchBox::setSearchBoxSize(Size size)
{
    m_size = size;
    applySize();
}

void AppSearchBox::applySize()
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

void AppSearchBox::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        m_debounceTimer->stop();
        emit searchSubmitted(text().trimmed());
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Escape) {
        clear();
        m_debounceTimer->stop();
        emit searchSubmitted("");
        event->accept();
        return;
    }
    QLineEdit::keyPressEvent(event);
}

} // namespace ui
