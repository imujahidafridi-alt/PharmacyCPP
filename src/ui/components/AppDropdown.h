#pragma once
#include <QComboBox>

namespace ui {

class AppDropdown : public QComboBox {
    Q_OBJECT
public:
    enum class Size {
        Small,   // 24px height
        Medium,  // 28px height
        Large    // 34px height
    };

    explicit AppDropdown(Size size = Size::Medium, QWidget* parent = nullptr);

    void setDropdownSize(Size size);
    Size dropdownSize() const { return m_size; }

    void setError(bool hasError);
    bool hasError() const { return m_hasError; }

    void addItemWithData(const QString& text, const QVariant& userData);
    bool selectByData(const QVariant& data);

private:
    void applySize();

    Size m_size;
    bool m_hasError = false;
};

} // namespace ui
