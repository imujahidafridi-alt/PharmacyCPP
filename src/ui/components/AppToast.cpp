#include "ui/components/AppToast.h"
#include <QHBoxLayout>
#include <QPushButton>
#include <QEvent>
#include <QPainter>
#include <QStyleOption>

namespace ui {

AppToast::AppToast(QWidget* parent, const QString& message, Type type, int durationMs)
    : QWidget(parent ? parent->window() : nullptr), m_parentWindow(parent ? parent->window() : nullptr)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_StyledBackground, true);

    QString borderColor, icon;
    switch (type) {
        case Type::Success:
            borderColor = "#22C55E";
            icon = "✓";
            break;
        case Type::Error:
            borderColor = "#EF4444";
            icon = "✕";
            break;
        case Type::Info:
            borderColor = "#38BDF8";
            icon = "ℹ";
            break;
        case Type::Warning:
            borderColor = "#F59E0B";
            icon = "⚠";
            break;
    }

    setStyleSheet(QString(R"(
        QWidget#toastContainer {
            background-color: #0F172A;
            border: 1.5px solid %1;
            border-radius: 8px;
        }
    )").arg(borderColor));

    setObjectName("toastContainer");
    setMinimumWidth(260);
    setMaximumWidth(520);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(14, 10, 12, 10);
    layout->setSpacing(10);

    auto* iconLabel = new QLabel(icon, this);
    iconLabel->setStyleSheet(QString("font-size: 15px; font-weight: 800; color: %1; background: transparent;").arg(borderColor));

    auto* textLabel = new QLabel(message, this);
    textLabel->setStyleSheet("font-size: 12px; font-weight: 600; color: #F8FAFC; background: transparent; line-height: 1.3;");
    textLabel->setWordWrap(true);

    auto* closeBtn = new QPushButton("✕", this);
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setFixedSize(18, 18);
    closeBtn->setStyleSheet(R"(
        QPushButton {
            background: transparent;
            border: none;
            color: #94A3B8;
            font-size: 11px;
            font-weight: 700;
            padding: 0px;
        }
        QPushButton:hover {
            color: #FFFFFF;
            background-color: rgba(255, 255, 255, 0.1);
            border-radius: 9px;
        }
    )");

    layout->addWidget(iconLabel);
    layout->addWidget(textLabel, 1);
    layout->addWidget(closeBtn);

    connect(closeBtn, &QPushButton::clicked, this, &QWidget::close);

    adjustSize();
    reposition();

    if (m_parentWindow) {
        m_parentWindow->installEventFilter(this);
    }

    m_opacityEffect = new QGraphicsOpacityEffect(this);
    setGraphicsEffect(m_opacityEffect);
    m_opacityEffect->setOpacity(1.0);

    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &AppToast::startFadeOut);
    m_timer->start(durationMs);

    show();
    raise();
}

void AppToast::paintEvent(QPaintEvent* event)
{
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
    QWidget::paintEvent(event);
}

void AppToast::reposition()
{
    if (!m_parentWindow) return;
    // Bottom-Center anchor with 45px margin from bottom edge
    int x = (m_parentWindow->width() - width()) / 2;
    int y = m_parentWindow->height() - height() - 45;
    if (x < 10) x = 10;
    if (y < 10) y = 10;
    move(x, y);
}

bool AppToast::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_parentWindow && (event->type() == QEvent::Resize || event->type() == QEvent::Move)) {
        reposition();
    }
    return QWidget::eventFilter(watched, event);
}

void AppToast::startFadeOut()
{
    m_fadeAnimation = new QPropertyAnimation(m_opacityEffect, "opacity", this);
    m_fadeAnimation->setDuration(350);
    m_fadeAnimation->setStartValue(1.0);
    m_fadeAnimation->setEndValue(0.0);
    connect(m_fadeAnimation, &QPropertyAnimation::finished, this, &QWidget::close);
    m_fadeAnimation->start(QAbstractAnimation::DeleteWhenStopped);
}

void AppToast::showSuccess(QWidget* parent, const QString& message, int durationMs)
{
    new AppToast(parent, message, Type::Success, durationMs);
}

void AppToast::showError(QWidget* parent, const QString& message, int durationMs)
{
    new AppToast(parent, message, Type::Error, durationMs);
}

void AppToast::showInfo(QWidget* parent, const QString& message, int durationMs)
{
    new AppToast(parent, message, Type::Info, durationMs);
}

void AppToast::showWarning(QWidget* parent, const QString& message, int durationMs)
{
    new AppToast(parent, message, Type::Warning, durationMs);
}

} // namespace ui
