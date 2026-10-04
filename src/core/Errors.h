#pragma once
#include <QString>

namespace core {

enum class ErrorCode {
    DatabaseConnectionLost,
    InsufficientStock,
    ItemNotFound,
    BarcodeAlreadyExists,
    ExpiredItemSaleBlocked,
    InvalidPaymentAmount,
    CustomerNotFound,
    SupplierNotFound,
    SaleNotFound,
    BatchNotFound,
    StockMovementFailed,
    PermissionDenied,
    PrinterUnavailable,
    GeneralDatabaseError,
    ValidationFailed,
    InvalidQuantity,
    Unknown
};

class AppError {
public:
    AppError(ErrorCode code, const QString& technicalDetail = QString(), const QString& friendlyMessage = QString());

    ErrorCode code() const { return m_code; }
    QString technicalDetail() const { return m_technicalDetail; }
    QString userMessage() const;

    static AppError fromSqlError(const QString& sqlError, const QString& context = QString());

private:
    ErrorCode m_code;
    QString m_technicalDetail;
    QString m_customMessage;
};

} // namespace core
