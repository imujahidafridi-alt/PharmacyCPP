#include "ui/components/MoneyInput.h"
#include <QDoubleValidator>
#include <QFocusEvent>
#include <QTimer>

namespace ui {

MoneyInput::MoneyInput(QWidget* parent) : QLineEdit(parent)
{
    setPlaceholderText("0.00");
    setAlignment(Qt::AlignRight);
    auto* val = new QDoubleValidator(0.0, 99999999.0, 2, this);
    val->setNotation(QDoubleValidator::StandardNotation);
    setValidator(val);

    connect(this, &QLineEdit::textChanged, this, [this](const QString&) {
        emit valueChanged(value());
    });
    connect(this, &QLineEdit::editingFinished, this, &MoneyInput::handleEditingFinished);
}

core::Money MoneyInput::value() const
{
    QString t = text().trimmed();
    t.remove("Rs.");
    t.remove(",");
    bool ok = false;
    double d = t.toDouble(&ok);
    if (!ok) return core::Money(0);
    return core::Money::fromRupees(d);
}

void MoneyInput::setValue(core::Money val)
{
    setText(val.formatted(false));
}

void MoneyInput::focusInEvent(QFocusEvent* event)
{
    QLineEdit::focusInEvent(event);
    QTimer::singleShot(0, this, &QLineEdit::selectAll);
}

void MoneyInput::handleEditingFinished()
{
    setValue(value());
    emit valueChanged(value());
}

} // namespace ui
