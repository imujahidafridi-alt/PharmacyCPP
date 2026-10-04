#pragma once
#include <QLineEdit>

namespace ui {

class AppTextInput : public QLineEdit {
    Q_OBJECT
public:
    enum class Size {
        Small,   // 24px height
        Medium,  // 28px height
        Large    // 34px height
    };

    explicit AppTextInput(const QString& placeholder = "", Size size = Size::Medium, QWidget* parent = nullptr);
    explicit AppTextInput(Size size, QWidget* parent = nullptr);
    explicit AppTextInput(QWidget* parent = nullptr);

    void setInputSize(Size size);
    Size inputSize() const { return m_size; }

    void setError(bool hasError, const QString& errorMessage = "");
    bool hasError() const { return m_hasError; }

signals:
    void submitted(const QString& text);

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    void applySize();

    Size m_size;
    bool m_hasError = false;
};

} // namespace ui
