#include "services/ReturnService.h"
#include "services/StockService.h"
#include "services/SaleService.h"
#include "database/DatabaseManager.h"
#include <QSqlQuery>
#include <QSqlError>

namespace services {

ReturnService& ReturnService::instance()
{
    static ReturnService inst;
    return inst;
}

core::Result<void, core::AppError> ReturnService::processSaleReturn(
    const QString& billNumber,
    const std::vector<ReturnItemRequest>& returnItems,
    const QString& reason,
    bool refundInCash,
    int userId)
{
    if (returnItems.empty()) {
        return core::Result<void, core::AppError>::err(
            core::AppError(core::ErrorCode::ValidationFailed, "Select at least one item to return.")
        );
    }

    auto saleRes = SaleService::instance().getSaleByBillNumber(billNumber);
    if (saleRes.isErr()) {
        return core::Result<void, core::AppError>::err(saleRes.error());
    }
    const auto& origSale = saleRes.value();

    core::Money totalRefund;
    for (const auto& r : returnItems) {
        totalRefund += r.refundAmount;
    }

    auto& dbMgr = database::DatabaseManager::instance();
    return dbMgr.executeTransaction([&](QSqlDatabase& db) -> core::Result<void, core::AppError> {
        // 1. Restock items and record movements
        for (const auto& r : returnItems) {
            auto stockRes = StockService::instance().applyMovement(
                r.itemId,
                r.batchId,
                domain::MovementType::SaleReturn,
                r.returnAtomicQty,
                QString("Return from Bill #%1: %2").arg(billNumber, reason),
                origSale.id,
                userId,
                db
            );
            if (stockRes.isErr()) return stockRes;
        }

        // 2. Adjust customer or cash
        if (!refundInCash && origSale.paymentType == domain::PaymentType::Udhaar && origSale.customerId > 1) {
            QSqlQuery q(db);
            q.prepare("UPDATE customers SET baqaya_paisa = baqaya_paisa - ? WHERE id = ?");
            q.addBindValue(totalRefund.paisa());
            q.addBindValue(origSale.customerId);
            q.exec();

            q.prepare(R"(
                INSERT INTO customer_ledger (customer_id, reference_type, reference_id, debit_paisa, credit_paisa, balance_after_paisa, description, user_id)
                VALUES (?, 'Sale Return', ?, 0, ?, (SELECT baqaya_paisa FROM customers WHERE id = ?), ?, ?)
            )");
            q.addBindValue(origSale.customerId);
            q.addBindValue(origSale.id);
            q.addBindValue(totalRefund.paisa());
            q.addBindValue(origSale.customerId);
            q.addBindValue(QString("Return Bill #%1: %2").arg(billNumber, reason));
            q.addBindValue(userId);
            q.exec();
        }

        // 3. Audit log
        QSqlQuery audit(db);
        audit.prepare("INSERT INTO audit_logs (user_id, action, entity, entity_id, new_value, reason) VALUES (?, 'SALE_RETURN', 'sales', ?, ?, ?)");
        audit.addBindValue(userId);
        audit.addBindValue(origSale.id);
        audit.addBindValue(QString("Refund: %1").arg(totalRefund.formatted()));
        audit.addBindValue(reason);
        audit.exec();

        return core::Result<void, core::AppError>::ok();
    });
}

} // namespace services
