#include "ui/components/QuantityInput.h"
#include <QHBoxLayout>

namespace ui {

QuantityInput::QuantityInput(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    m_minusBtn = new QPushButton("-", this);
    m_minusBtn->setFixedSize(28, 28);
    m_minusBtn->setProperty("class", "secondaryBtn");

    m_spinBox = new QSpinBox(this);
    m_spinBox->setRange(1, 9999);
    m_spinBox->setValue(1);
    m_spinBox->setAlignment(Qt::AlignCenter);
    m_spinBox->setFixedHeight(28);

    m_plusBtn = new QPushButton("+", this);
    m_plusBtn->setFixedSize(28, 28);
    m_plusBtn->setProperty("class", "secondaryBtn");

    layout->addWidget(m_minusBtn);
    layout->addWidget(m_spinBox);
    layout->addWidget(m_plusBtn);

    connect(m_minusBtn, &QPushButton::clicked, this, [this]() {
        if (m_spinBox->value() > m_spinBox->minimum()) {
            m_spinBox->setValue(m_spinBox->value() - 1);
        }
    });

    connect(m_plusBtn, &QPushButton::clicked, this, [this]() {
        if (m_spinBox->value() < m_spinBox->maximum()) {
            m_spinBox->setValue(m_spinBox->value() + 1);
        }
    });

    connect(m_spinBox, &QSpinBox::valueChanged, this, &QuantityInput::valueChanged);
}

int QuantityInput::value() const
{
    return m_spinBox->value();
}

void QuantityInput::setValue(int val)
{
    m_spinBox->setValue(val);
}

void QuantityInput::setRange(int min, int max)
{
    m_spinBox->setRange(min, max);
}

} // namespace ui
