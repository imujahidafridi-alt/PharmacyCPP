#pragma once
#include <QLineEdit>
#include "core/Money.h"

namespace ui {

class MoneyInput : public QLineEdit {
    Q_OBJECT
public:
    explicit MoneyInput(QWidget* parent = nullptr);

    core::Money value() const;
    void setValue(core::Money val);

signals:
    void valueChanged(core::Money val);

private slots:
    void handleEditingFinished();
};

} // namespace ui
