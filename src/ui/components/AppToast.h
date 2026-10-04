#pragma once
#include <QWidget>
#include <QLabel>
#include <QTimer>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>

namespace ui {

class AppToast : public QWidget {
    Q_OBJECT
public:
    enum class Type {
        Success,
        Error,
        Info,
        Warning
    };

    static void showSuccess(QWidget* parent, const QString& message, int durationMs = 3000);
    static void showError(QWidget* parent, const QString& message, int durationMs = 4000);
    static void showInfo(QWidget* parent, const QString& message, int durationMs = 3000);
    static void showWarning(QWidget* parent, const QString& message, int durationMs = 3500);

private:
    AppToast(QWidget* parent, const QString& message, Type type, int durationMs);

    void reposition();
    void startFadeOut();

    bool eventFilter(QObject* watched, QEvent* event) override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QWidget* m_parentWindow{nullptr};
    QTimer* m_timer{nullptr};
    QGraphicsOpacityEffect* m_opacityEffect{nullptr};
    QPropertyAnimation* m_fadeAnimation{nullptr};
};

} // namespace ui
