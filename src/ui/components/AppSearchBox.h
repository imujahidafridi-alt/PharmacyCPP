#pragma once
#include <QLineEdit>
#include <QTimer>

namespace ui {

class AppSearchBox : public QLineEdit {
    Q_OBJECT
public:
    enum class Size {
        Small,   // 24px height
        Medium,  // 28px height
        Large    // 34px height
    };

    explicit AppSearchBox(const QString& placeholder = "Search...", Size size = Size::Medium, QWidget* parent = nullptr);

    void setSearchBoxSize(Size size);
    Size searchBoxSize() const { return m_size; }

    void setDebounceDelay(int ms) { m_debounceMs = ms; }

signals:
    void searchDebounced(const QString& text);
    void searchSubmitted(const QString& text);

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    void applySize();

    Size m_size;
    QTimer* m_debounceTimer;
    int m_debounceMs = 250;
};

} // namespace ui
