#pragma once
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include "ui/components/AppButton.h"

namespace ui {

class AppModal : public QDialog {
    Q_OBJECT
public:
    explicit AppModal(const QString& title, QWidget* parent = nullptr);

    void setHeader(const QString& title, const QString& subtitle = "", const QString& icon = "");
    
    QVBoxLayout* contentLayout() const { return m_contentLayout; }
    QHBoxLayout* footerLayout() const { return m_footerLayout; }

    AppButton* confirmButton() const { return m_confirmBtn; }
    AppButton* cancelButton() const { return m_cancelBtn; }

    void setConfirmButton(const QString& text, AppButton::Variant variant = AppButton::Variant::Primary);
    void setCancelButton(const QString& text);
    void addFooterButton(AppButton* btn, bool beforeConfirm = true);

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    void initLayout();

    QVBoxLayout* m_mainLayout;
    QVBoxLayout* m_contentLayout;
    QHBoxLayout* m_footerLayout;

    QWidget* m_headerWidget;
    QLabel* m_iconLabel;
    QLabel* m_titleLabel;
    QLabel* m_subtitleLabel;

    AppButton* m_cancelBtn;
    AppButton* m_confirmBtn;
};

} // namespace ui
