#include "ui/components/AppButton.h"
#include <QStyle>

namespace ui {

AppButton::AppButton(const QString& text, Variant variant, Size size, QWidget* parent)
    : QPushButton(text, parent), m_variant(variant), m_size(size)
{
    setCursor(Qt::PointingHandCursor);
    applyVariantAndSize();
}

AppButton::AppButton(QWidget* parent)
    : QPushButton(parent), m_variant(Variant::Secondary), m_size(Size::Medium)
{
    setCursor(Qt::PointingHandCursor);
    applyVariantAndSize();
}

void AppButton::setVariant(Variant variant)
{
    m_variant = variant;
    applyVariantAndSize();
}

void AppButton::setButtonSize(Size size)
{
    m_size = size;
    applyVariantAndSize();
}

void AppButton::applyVariantAndSize()
{
    // Apply class property for stylesheet targeting
    switch (m_variant) {
        case Variant::Primary:
            setProperty("class", "primaryBtn");
            break;
        case Variant::Secondary:
            setProperty("class", "secondaryBtn");
            break;
        case Variant::Danger:
            setProperty("class", "dangerBtn");
            break;
        case Variant::Info:
            setProperty("class", "infoBtn");
            break;
        case Variant::Success:
            setProperty("class", "successBtn");
            break;
    }

    // Apply height based on density size
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

    // Refresh styling
    style()->unpolish(this);
    style()->polish(this);
    update();
}

AppButton* AppButton::primary(const QString& text, Size size, QWidget* parent)
{
    return new AppButton(text, Variant::Primary, size, parent);
}

AppButton* AppButton::secondary(const QString& text, Size size, QWidget* parent)
{
    return new AppButton(text, Variant::Secondary, size, parent);
}

AppButton* AppButton::danger(const QString& text, Size size, QWidget* parent)
{
    return new AppButton(text, Variant::Danger, size, parent);
}

AppButton* AppButton::info(const QString& text, Size size, QWidget* parent)
{
    return new AppButton(text, Variant::Info, size, parent);
}

AppButton* AppButton::success(const QString& text, Size size, QWidget* parent)
{
    return new AppButton(text, Variant::Success, size, parent);
}

} // namespace ui
