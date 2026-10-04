#pragma once
#include <QPushButton>

namespace ui {

class AppButton : public QPushButton {
    Q_OBJECT
public:
    enum class Variant {
        Primary,
        Secondary,
        Danger,
        Info,
        Success
    };

    enum class Size {
        Small,   // 24px height
        Medium,  // 28px height
        Large    // 34px height
    };

    explicit AppButton(const QString& text, Variant variant = Variant::Secondary, Size size = Size::Medium, QWidget* parent = nullptr);
    explicit AppButton(QWidget* parent = nullptr);

    void setVariant(Variant variant);
    Variant variant() const { return m_variant; }

    void setButtonSize(Size size);
    Size buttonSize() const { return m_size; }

    // Static helper factories
    static AppButton* primary(const QString& text, Size size = Size::Medium, QWidget* parent = nullptr);
    static AppButton* secondary(const QString& text, Size size = Size::Medium, QWidget* parent = nullptr);
    static AppButton* danger(const QString& text, Size size = Size::Medium, QWidget* parent = nullptr);
    static AppButton* info(const QString& text, Size size = Size::Medium, QWidget* parent = nullptr);
    static AppButton* success(const QString& text, Size size = Size::Medium, QWidget* parent = nullptr);

private:
    void applyVariantAndSize();

    Variant m_variant;
    Size m_size;
};

} // namespace ui
