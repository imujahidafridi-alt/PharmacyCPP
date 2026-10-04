#include "core/Errors.h"

namespace core {

AppError::AppError(ErrorCode code, const QString& technicalDetail, const QString& friendlyMessage)
    : m_code(code), m_technicalDetail(technicalDetail), m_customMessage(friendlyMessage)
{
}

QString AppError::userMessage() const
{
    if (!m_customMessage.isEmpty()) {
        return m_customMessage;
    }

    switch (m_code) {
    case ErrorCode::DatabaseConnectionLost:
        return "Store database is temporarily unavailable. Please check the network connection.";
    case ErrorCode::InsufficientStock:
        return "Insufficient stock available for this item.";
    case ErrorCode::ItemNotFound:
        return "Item not found.";
    case ErrorCode::BarcodeAlreadyExists:
        return "An item with this barcode already exists.";
    case ErrorCode::ExpiredItemSaleBlocked:
        return "Expired items cannot be sold.";
    case ErrorCode::InvalidPaymentAmount:
        return "Received cash cannot be less than total amount.";
    case ErrorCode::CustomerNotFound:
        return "Customer account not found.";
    case ErrorCode::SupplierNotFound:
        return "Supplier account not found.";
    case ErrorCode::SaleNotFound:
        return "Sale bill not found.";
    case ErrorCode::BatchNotFound:
        return "Item batch not found.";
    case ErrorCode::StockMovementFailed:
        return "Stock balance could not be updated. Transaction cancelled.";
    case ErrorCode::PermissionDenied:
        return "You do not have permission to perform this action.";
    case ErrorCode::PrinterUnavailable:
        return "Receipt printer is not connected or out of paper.";
    case ErrorCode::ValidationFailed:
        return "Please fill in all required fields correctly.";
    case ErrorCode::InvalidQuantity:
        return "Please enter a valid quantity.";
    case ErrorCode::GeneralDatabaseError:
    case ErrorCode::Unknown:
    default:
        return "An unexpected system error occurred. Please try again or contact manager.";
    }
}

AppError AppError::fromSqlError(const QString& sqlError, const QString& context)
{
    QString errLower = sqlError.toLower();
    if (errLower.contains("foreign key") || errLower.contains("constraint")) {
        return AppError(ErrorCode::ValidationFailed, sqlError,
                        "This record cannot be changed or removed because it is referenced in past transactions.");
    }
    if (errLower.contains("unique") || errLower.contains("duplicate key")) {
        return AppError(ErrorCode::BarcodeAlreadyExists, sqlError,
                        "An item with this code or barcode already exists in the system.");
    }
    if (errLower.contains("connection") || errLower.contains("timeout") || errLower.contains("could not connect")) {
        return AppError(ErrorCode::DatabaseConnectionLost, sqlError,
                        "Store database is temporarily unavailable. Please check network connection.");
    }
    return AppError(ErrorCode::GeneralDatabaseError, sqlError,
                    context.isEmpty() ? "Database operation failed." : context);
}

} // namespace core
