#pragma once
#include <QWidget>
#include <QSpinBox>
#include <QPushButton>

namespace ui {

class QuantityInput : public QWidget {
    Q_OBJECT
public:
    explicit QuantityInput(QWidget* parent = nullptr);

    int value() const;
    void setValue(int val);
    void setRange(int min, int max);

signals:
    void valueChanged(int val);

private:
    QSpinBox* m_spinBox;
    QPushButton* m_plusBtn;
    QPushButton* m_minusBtn;
};

} // namespace ui
