#pragma once
#include "ui/components/AppModal.h"
#include "domain/Models.h"
#include <QTextEdit>
#include <optional>

namespace ui {

class ReceiptPreviewDialog : public AppModal {
    Q_OBJECT
public:
    explicit ReceiptPreviewDialog(const domain::Sale& sale, QWidget* parent = nullptr);
    explicit ReceiptPreviewDialog(const QString& receiptText, const QString& title, QWidget* parent = nullptr);

private slots:
    void handlePrint();

private:
    void setupUI(const QString& receiptText);
    std::optional<domain::Sale> m_sale;
    QString m_rawText;
    QTextEdit* m_receiptDisplay{nullptr};
};

} // namespace ui
