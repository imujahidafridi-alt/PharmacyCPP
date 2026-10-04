#pragma once
#include <QString>
#include <QByteArray>
#include "domain/Models.h"

namespace printing {

class ReceiptRenderer {
public:
    static QByteArray renderEscPos(const domain::Sale& sale);
    static QString renderPlainText(const domain::Sale& sale);
    static QString renderKhataStatement(const domain::Customer& customer, const std::vector<domain::CartItem>& recentItems);
    static QString renderZReportPlainText(const domain::CashSession& session);
    static QByteArray renderZReportEscPos(const domain::CashSession& session);
};

} // namespace printing
