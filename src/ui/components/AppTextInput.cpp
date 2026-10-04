#include "ui/components/AppTextInput.h"
#include <QKeyEvent>
#include <QStyle>

namespace ui {

AppTextInput::AppTextInput(const QString& placeholder, Size size, QWidget* parent)
    : QLineEdit(parent), m_size(size)
{
    setPlaceholderText(placeholder);
    setClearButtonEnabled(true);
    applySize();
}

AppTextInput::AppTextInput(Size size, QWidget* parent)
    : QLineEdit(parent), m_size(size)
{
    setClearButtonEnabled(true);
    applySize();
}

AppTextInput::AppTextInput(QWidget* parent)
    : QLineEdit(parent), m_size(Size::Medium)
{
    setClearButtonEnabled(true);
    applySize();
}

void AppTextInput::setInputSize(Size size)
{
    m_size = size;
    applySize();
}

void AppTextInput::applySize()
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

void AppTextInput::setError(bool hasError, const QString& errorMessage)
{
    m_hasError = hasError;
    setProperty("hasError", hasError);
    setToolTip(hasError ? errorMessage : "");
    style()->unpolish(this);
    style()->polish(this);
    update();
}

void AppTextInput::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        emit submitted(text().trimmed());
    }
    QLineEdit::keyPressEvent(event);
}

} // namespace ui
