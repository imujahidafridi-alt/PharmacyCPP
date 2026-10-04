#include "ui/components/AppModal.h"
#include <QKeyEvent>
#include <QFrame>

namespace ui {

AppModal::AppModal(const QString& title, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(title);
    setStyleSheet("QDialog { background-color: #FFFFFF; }");
    initLayout();
    setHeader(title);
}

void AppModal::initLayout()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(18, 18, 18, 18);
    m_mainLayout->setSpacing(14);

    // 1. Header
    m_headerWidget = new QWidget(this);
    auto* headerLayout = new QVBoxLayout(m_headerWidget);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(3);

    auto* titleRow = new QHBoxLayout();
    titleRow->setContentsMargins(0, 0, 0, 0);
    titleRow->setSpacing(8);

    m_iconLabel = new QLabel(this);
    m_iconLabel->setVisible(false);
    m_iconLabel->setStyleSheet("font-size: 18px;");

    m_titleLabel = new QLabel(this);
    m_titleLabel->setStyleSheet("font-size: 15px; font-weight: 700; color: #0F172A;");

    titleRow->addWidget(m_iconLabel);
    titleRow->addWidget(m_titleLabel);
    titleRow->addStretch();
    headerLayout->addLayout(titleRow);

    m_subtitleLabel = new QLabel(this);
    m_subtitleLabel->setStyleSheet("font-size: 11px; color: #64748B;");
    m_subtitleLabel->setVisible(false);
    m_subtitleLabel->setWordWrap(true);
    headerLayout->addWidget(m_subtitleLabel);

    m_mainLayout->addWidget(m_headerWidget);

    // 2. Body / Content layout
    m_contentLayout = new QVBoxLayout();
    m_contentLayout->setContentsMargins(0, 0, 0, 0);
    m_contentLayout->setSpacing(10);
    m_mainLayout->addLayout(m_contentLayout, 1);

    // 3. Footer separator and buttons
    auto* line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet("color: #E2E8F0; background-color: #E2E8F0; max-height: 1px;");
    m_mainLayout->addWidget(line);

    m_footerLayout = new QHBoxLayout();
    m_footerLayout->setContentsMargins(0, 0, 0, 0);
    m_footerLayout->setSpacing(8);

    m_cancelBtn = AppButton::secondary("Cancel (Esc)", AppButton::Size::Medium, this);
    m_confirmBtn = AppButton::primary("Confirm (Enter)", AppButton::Size::Medium, this);
    m_confirmBtn->setDefault(true);

    m_footerLayout->addWidget(m_cancelBtn, 1);
    m_footerLayout->addWidget(m_confirmBtn, 1);
    m_mainLayout->addLayout(m_footerLayout);

    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_confirmBtn, &QPushButton::clicked, this, &QDialog::accept);
}

void AppModal::setHeader(const QString& title, const QString& subtitle, const QString& icon)
{
    m_titleLabel->setText(title);

    if (!icon.isEmpty()) {
        m_iconLabel->setText(icon);
        m_iconLabel->setVisible(true);
    } else {
        m_iconLabel->setVisible(false);
    }

    if (!subtitle.isEmpty()) {
        m_subtitleLabel->setText(subtitle);
        m_subtitleLabel->setVisible(true);
    } else {
        m_subtitleLabel->setVisible(false);
    }
}

void AppModal::setConfirmButton(const QString& text, AppButton::Variant variant)
{
    m_confirmBtn->setText(text);
    m_confirmBtn->setVariant(variant);
    m_confirmBtn->setVisible(!text.isEmpty());
}

void AppModal::setCancelButton(const QString& text)
{
    m_cancelBtn->setText(text);
    m_cancelBtn->setVisible(!text.isEmpty());
}

void AppModal::addFooterButton(AppButton* btn, bool beforeConfirm)
{
    if (beforeConfirm) {
        m_footerLayout->insertWidget(m_footerLayout->indexOf(m_confirmBtn), btn, 1);
    } else {
        m_footerLayout->addWidget(btn, 1);
    }
}

void AppModal::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        reject();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (m_confirmBtn && m_confirmBtn->isVisible() && m_confirmBtn->isEnabled()) {
            accept();
            event->accept();
            return;
        }
    }
    QDialog::keyPressEvent(event);
}

} // namespace ui
