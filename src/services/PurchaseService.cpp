#include "services/PurchaseService.h"
#include "services/StockService.h"
#include "database/DatabaseManager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>

namespace services {

PurchaseService& PurchaseService::instance()
{
    static PurchaseService inst;
    return inst;
}

core::Result<domain::Purchase, core::AppError> PurchaseService::recordPurchase(domain::Purchase purchase, int userId)
{
    if (purchase.items.empty()) {
        return core::Result<domain::Purchase, core::AppError>::err(
            core::AppError(core::ErrorCode::ValidationFailed, "Purchase must contain at least one item.")
        );
    }

    purchase.createdAt = QDateTime::currentDateTime();
    purchase.userId = userId;

    auto& dbMgr = database::DatabaseManager::instance();
    int newPurchaseId = 0;

    auto res = dbMgr.executeTransaction([&](QSqlDatabase& db) -> core::Result<void, core::AppError> {
        QSqlQuery q(db);

        // 1. Insert Purchase Header
        q.prepare(R"(
            INSERT INTO purchases (invoice_number, supplier_id, is_cash_market, total_cost_paisa, amount_paid_paisa, notes, user_id, created_at)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?)
        )");
        q.addBindValue(purchase.invoiceNumber);
        q.addBindValue(purchase.supplierId > 0 ? QVariant(purchase.supplierId) : QVariant());
        q.addBindValue(purchase.isCashMarketPurchase ? 1 : 0);
        q.addBindValue(purchase.totalCost.paisa());
        q.addBindValue(purchase.amountPaid.paisa());
        q.addBindValue(purchase.notes);
        q.addBindValue(userId);
        q.addBindValue(purchase.createdAt);

        if (!q.exec()) {
            return core::Result<void, core::AppError>::err(
                core::AppError::fromSqlError(q.lastError().text(), "Failed to record purchase.")
            );
        }

        newPurchaseId = q.lastInsertId().toInt();
        purchase.id = newPurchaseId;

        // 2. Process Items and Batches
        for (const auto& item : purchase.items) {
            // Find or create batch
            int batchId = 0;
            QString batchNum = item.batchNumber.trimmed().isEmpty() ? "DEFAULT" : item.batchNumber.trimmed();
            QDate expDate = item.expiryDate.isValid() ? item.expiryDate : QDate::currentDate().addYears(2);

            q.prepare("SELECT id FROM batches WHERE item_id = ? AND batch_number = ?");
            q.addBindValue(item.itemId);
            q.addBindValue(batchNum);
            if (q.exec() && q.next()) {
                batchId = q.value(0).toInt();
                // Update batch cost and expiry
                QSqlQuery updateBatch(db);
                updateBatch.prepare("UPDATE batches SET cost_price_paisa = ?, expiry_date = ? WHERE id = ?");
                updateBatch.addBindValue(item.unitCost.paisa());
                updateBatch.addBindValue(expDate);
                updateBatch.addBindValue(batchId);
                updateBatch.exec();
            } else {
                // Insert new batch
                QSqlQuery insertBatch(db);
                insertBatch.prepare(R"(
                    INSERT INTO batches (item_id, batch_number, expiry_date, cost_price_paisa, sale_price_paisa)
                    VALUES (?, ?, ?, ?, ?)
                )");
                insertBatch.addBindValue(item.itemId);
                insertBatch.addBindValue(batchNum);
                insertBatch.addBindValue(expDate);
                insertBatch.addBindValue(item.unitCost.paisa());
                insertBatch.addBindValue(item.unitCost.paisa()); // default sale price same or higher
                if (insertBatch.exec()) {
                    batchId = insertBatch.lastInsertId().toInt();
                }
            }

            // Insert into purchase_items
            q.prepare(R"(
                INSERT INTO purchase_items (purchase_id, item_id, batch_id, atomic_qty, unit_cost_paisa, total_cost_paisa)
                VALUES (?, ?, ?, ?, ?, ?)
            )");
            q.addBindValue(newPurchaseId);
            q.addBindValue(item.itemId);
            q.addBindValue(batchId > 0 ? QVariant(batchId) : QVariant());
            q.addBindValue(item.atomicQty);
            q.addBindValue(item.unitCost.paisa());
            q.addBindValue(item.totalCost.paisa());
            if (!q.exec()) {
                return core::Result<void, core::AppError>::err(core::AppError::fromSqlError(q.lastError().text()));
            }

            // Increment stock atomically
            auto stockRes = StockService::instance().applyMovement(
                item.itemId,
                batchId,
                domain::MovementType::Purchase,
                item.atomicQty,
                purchase.isCashMarketPurchase ? "Cash Market Purchase" : QString("Purchase Inv #%1").arg(purchase.invoiceNumber),
                newPurchaseId,
                userId,
                db
            );
            if (stockRes.isErr()) return stockRes;
        }

        // 3. Update Supplier Ledger if supplier is attached
        if (purchase.supplierId > 1 && !purchase.isCashMarketPurchase) {
            int64_t unpaidPaisa = purchase.totalCost.paisa() - purchase.amountPaid.paisa();
            if (unpaidPaisa != 0) {
                q.prepare("UPDATE suppliers SET baqaya_paisa = baqaya_paisa + ? WHERE id = ?");
                q.addBindValue(unpaidPaisa);
                q.addBindValue(purchase.supplierId);
                q.exec();

                q.prepare(R"(
                    INSERT INTO supplier_ledger (supplier_id, reference_type, reference_id, debit_paisa, credit_paisa, balance_after_paisa, description, user_id)
                    VALUES (?, 'Purchase', ?, ?, ?, (SELECT baqaya_paisa FROM suppliers WHERE id = ?), ?, ?)
                )");
                q.addBindValue(purchase.supplierId);
                q.addBindValue(newPurchaseId);
                q.addBindValue(purchase.amountPaid.paisa());
                q.addBindValue(purchase.totalCost.paisa());
                q.addBindValue(purchase.supplierId);
                q.addBindValue(QString("Purchase Inv #%1").arg(purchase.invoiceNumber));
                q.addBindValue(userId);
                q.exec();
            }
        }

        return core::Result<void, core::AppError>::ok();
    });

    if (res.isErr()) {
        return core::Result<domain::Purchase, core::AppError>::err(res.error());
    }

    return core::Result<domain::Purchase, core::AppError>::ok(purchase);
}

core::Result<void, core::AppError> PurchaseService::recordPurchaseReturn(
    int supplierId,
    int itemId,
    int batchId,
    int returnQty,
    core::Money refundAmount,
    const QString& reason,
    int userId)
{
    if (returnQty <= 0) {
        return core::Result<void, core::AppError>::err(core::AppError(core::ErrorCode::InvalidQuantity));
    }

    auto& dbMgr = database::DatabaseManager::instance();
    return dbMgr.executeTransaction([&](QSqlDatabase& db) -> core::Result<void, core::AppError> {
        // Decrement stock
        auto stockRes = StockService::instance().applyMovement(
            itemId,
            batchId,
            domain::MovementType::PurchaseReturn,
            -returnQty,
            QString("Purchase Return: %1").arg(reason),
            0,
            userId,
            db
        );
        if (stockRes.isErr()) return stockRes;

        // Adjust supplier balance if supplier provided
        if (supplierId > 0) {
            QSqlQuery q(db);
            q.prepare("UPDATE suppliers SET baqaya_paisa = baqaya_paisa - ? WHERE id = ?");
            q.addBindValue(refundAmount.paisa());
            q.addBindValue(supplierId);
            q.exec();

            q.prepare(R"(
                INSERT INTO supplier_ledger (supplier_id, reference_type, reference_id, debit_paisa, credit_paisa, balance_after_paisa, description, user_id)
                VALUES (?, 'Purchase Return', 0, ?, 0, (SELECT baqaya_paisa FROM suppliers WHERE id = ?), ?, ?)
            )");
            q.addBindValue(supplierId);
            q.addBindValue(refundAmount.paisa());
            q.addBindValue(supplierId);
            q.addBindValue(QString("Return: %1").arg(reason));
            q.addBindValue(userId);
            q.exec();
        }

        return core::Result<void, core::AppError>::ok();
    });
}

core::Result<std::vector<domain::Purchase>, core::AppError> PurchaseService::getRecentPurchases(int limit)
{
    auto& dbMgr = database::DatabaseManager::instance();
    QSqlDatabase db = dbMgr.connection();
    if (!db.isOpen()) {
        return core::Result<std::vector<domain::Purchase>, core::AppError>::err(core::AppError(core::ErrorCode::DatabaseConnectionLost));
    }

    QSqlQuery q(db);
    q.prepare(R"(
        SELECT p.*, s.name AS supplier_name
        FROM purchases p
        LEFT JOIN suppliers s ON p.supplier_id = s.id
        ORDER BY p.created_at DESC
        LIMIT ?
    )");
    q.addBindValue(limit);

    if (!q.exec()) {
        return core::Result<std::vector<domain::Purchase>, core::AppError>::err(core::AppError::fromSqlError(q.lastError().text()));
    }

    std::vector<domain::Purchase> purchases;
    while (q.next()) {
        domain::Purchase p;
        p.id = q.value("id").toInt();
        p.invoiceNumber = q.value("invoice_number").toString();
        p.supplierId = q.value("supplier_id").toInt();
        p.supplierName = q.value("supplier_name").toString();
        p.isCashMarketPurchase = (q.value("is_cash_market").toInt() == 1);
        p.totalCost = core::Money::fromPaisa(q.value("total_cost_paisa").toLongLong());
        p.amountPaid = core::Money::fromPaisa(q.value("amount_paid_paisa").toLongLong());
        p.notes = q.value("notes").toString();
        p.createdAt = q.value("created_at").toDateTime();
        purchases.push_back(std::move(p));
    }

    return core::Result<std::vector<domain::Purchase>, core::AppError>::ok(std::move(purchases));
}

} // namespace services
