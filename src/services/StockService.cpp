#include "services/StockService.h"
#include "database/DatabaseManager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDate>
#include <algorithm>

namespace services {

StockService& StockService::instance()
{
    static StockService inst;
    return inst;
}

core::Result<std::vector<domain::StockItemView>, core::AppError> StockService::getStockOverview(
    const QString& searchFilter,
    int categoryFilter)
{
    auto& dbMgr = database::DatabaseManager::instance();
    QSqlDatabase db = dbMgr.connection();
    if (!db.isOpen()) {
        return core::Result<std::vector<domain::StockItemView>, core::AppError>::err(
            core::AppError(core::ErrorCode::DatabaseConnectionLost)
        );
    }

    QString sql = R"(
        SELECT 
            i.id, i.code, i.name, c.name AS category_name,
            COALESCE(SUM(sb.quantity), 0) AS total_qty,
            i.min_stock_alert,
            i.sale_price_paisa,
            MIN(b.expiry_date) AS nearest_expiry,
            (SELECT batch_number FROM batches WHERE item_id = i.id ORDER BY expiry_date ASC LIMIT 1) AS nearest_batch
        FROM items i
        LEFT JOIN categories c ON i.category_id = c.id
        LEFT JOIN stock_balances sb ON i.id = sb.item_id
        LEFT JOIN batches b ON sb.batch_id = b.id AND sb.quantity > 0
        WHERE i.is_active = 1
    )";

    QVariantList binds;
    if (!searchFilter.trimmed().isEmpty()) {
        sql += " AND (i.name LIKE ? OR i.code LIKE ? OR i.barcode LIKE ? OR i.generic_name LIKE ?)";
        QString wildcard = "%" + searchFilter.trimmed() + "%";
        binds << wildcard << wildcard << wildcard << wildcard;
    }
    if (categoryFilter > 0) {
        sql += " AND i.category_id = ?";
        binds << categoryFilter;
    }

    sql += " GROUP BY i.id, i.code, i.name, c.name, i.min_stock_alert, i.sale_price_paisa ORDER BY i.name ASC";

    QSqlQuery q(db);
    q.prepare(sql);
    for (const auto& b : binds) {
        q.addBindValue(b);
    }

    if (!q.exec()) {
        return core::Result<std::vector<domain::StockItemView>, core::AppError>::err(
            core::AppError::fromSqlError(q.lastError().text())
        );
    }

    std::vector<domain::StockItemView> items;
    QDate today = QDate::currentDate();

    while (q.next()) {
        domain::StockItemView item;
        item.itemId = q.value("id").toInt();
        item.code = q.value("code").toString();
        item.itemName = q.value("name").toString();
        item.categoryName = q.value("category_name").toString();
        item.totalAtomicQty = q.value("total_qty").toInt();
        item.minStock = q.value("min_stock_alert").toInt();
        item.retailPrice = core::Money::fromPaisa(q.value("sale_price_paisa").toLongLong());

        QVariant expVal = q.value("nearest_expiry");
        if (expVal.isValid() && !expVal.isNull()) {
            item.nearestExpiry = expVal.toDate();
            item.nearestBatch = q.value("nearest_batch").toString();
        }

        // Determine stock status
        if (item.totalAtomicQty <= 0) {
            item.status = domain::StockStatus::OutOfStock;
        } else if (item.nearestExpiry.has_value() && *item.nearestExpiry < today) {
            item.status = domain::StockStatus::Expired;
        } else if (item.nearestExpiry.has_value() && *item.nearestExpiry <= today.addDays(60)) {
            item.status = domain::StockStatus::NearExpiry;
        } else if (item.totalAtomicQty <= item.minStock) {
            item.status = domain::StockStatus::LowStock;
        } else {
            item.status = domain::StockStatus::Available;
        }

        items.push_back(std::move(item));
    }

    return core::Result<std::vector<domain::StockItemView>, core::AppError>::ok(std::move(items));
}

core::Result<std::vector<domain::Batch>, core::AppError> StockService::getItemBatches(int itemId)
{
    auto& dbMgr = database::DatabaseManager::instance();
    QSqlDatabase db = dbMgr.connection();
    if (!db.isOpen()) {
        return core::Result<std::vector<domain::Batch>, core::AppError>::err(
            core::AppError(core::ErrorCode::DatabaseConnectionLost)
        );
    }

    QSqlQuery q(db);
    q.prepare(R"(
        SELECT b.id, b.item_id, b.batch_number, b.expiry_date, b.cost_price_paisa, b.sale_price_paisa,
               COALESCE(sb.quantity, 0) AS qty
        FROM batches b
        LEFT JOIN stock_balances sb ON b.id = sb.batch_id AND sb.item_id = b.item_id
        WHERE b.item_id = ?
        ORDER BY b.expiry_date ASC
    )");
    q.addBindValue(itemId);

    if (!q.exec()) {
        return core::Result<std::vector<domain::Batch>, core::AppError>::err(
            core::AppError::fromSqlError(q.lastError().text())
        );
    }

    std::vector<domain::Batch> batches;
    while (q.next()) {
        domain::Batch batch;
        batch.id = q.value("id").toInt();
        batch.itemId = q.value("item_id").toInt();
        batch.batchNumber = q.value("batch_number").toString();
        batch.expiryDate = q.value("expiry_date").toDate();
        batch.costPrice = core::Money::fromPaisa(q.value("cost_price_paisa").toLongLong());
        batch.salePrice = core::Money::fromPaisa(q.value("sale_price_paisa").toLongLong());
        batch.quantityRemaining = q.value("qty").toInt();
        batches.push_back(std::move(batch));
    }

    return core::Result<std::vector<domain::Batch>, core::AppError>::ok(std::move(batches));
}

core::Result<std::vector<FefoAllocation>, core::AppError> StockService::allocateFefo(
    int itemId,
    int requestedAtomicQty,
    bool allowExpired)
{
    if (requestedAtomicQty <= 0) {
        return core::Result<std::vector<FefoAllocation>, core::AppError>::err(
            core::AppError(core::ErrorCode::InvalidQuantity)
        );
    }

    auto batchesRes = getItemBatches(itemId);
    if (batchesRes.isErr()) {
        return core::Result<std::vector<FefoAllocation>, core::AppError>::err(batchesRes.error());
    }

    const auto& batches = batchesRes.value();
    QDate today = QDate::currentDate();
    std::vector<FefoAllocation> allocations;
    int remainingToAllocate = requestedAtomicQty;

    for (const auto& batch : batches) {
        if (batch.quantityRemaining <= 0) continue;

        bool expired = (batch.expiryDate < today);
        if (expired && !allowExpired) {
            // Expired items cannot be automatically allocated for sale (Scenario 8)
            continue;
        }

        int take = std::min(remainingToAllocate, batch.quantityRemaining);
        FefoAllocation alloc;
        alloc.batchId = batch.id;
        alloc.batchNumber = batch.batchNumber;
        alloc.expiryDate = batch.expiryDate;
        alloc.allocatedQty = take;
        alloc.costPrice = batch.costPrice;
        alloc.salePrice = batch.salePrice;
        alloc.isExpired = expired;

        allocations.push_back(alloc);
        remainingToAllocate -= take;

        if (remainingToAllocate == 0) break;
    }

    if (remainingToAllocate > 0) {
        return core::Result<std::vector<FefoAllocation>, core::AppError>::err(
            core::AppError(core::ErrorCode::InsufficientStock)
        );
    }

    return core::Result<std::vector<FefoAllocation>, core::AppError>::ok(std::move(allocations));
}

core::Result<void, core::AppError> StockService::correctStock(
    int itemId,
    int batchId,
    int physicalCount,
    const QString& reason,
    int userId)
{
    if (physicalCount < 0) {
        return core::Result<void, core::AppError>::err(
            core::AppError(core::ErrorCode::InvalidQuantity, "Physical count cannot be negative.")
        );
    }
    if (reason.trimmed().isEmpty()) {
        return core::Result<void, core::AppError>::err(
            core::AppError(core::ErrorCode::ValidationFailed, "Mandatory reason required for stock correction.")
        );
    }

    auto& dbMgr = database::DatabaseManager::instance();
    return dbMgr.executeTransaction([&](QSqlDatabase& db) -> core::Result<void, core::AppError> {
        QSqlQuery q(db);
        q.prepare("SELECT quantity FROM stock_balances WHERE item_id = ? AND batch_id = ?");
        q.addBindValue(itemId);
        q.addBindValue(batchId);
        if (!q.exec() || !q.next()) {
            return core::Result<void, core::AppError>::err(
                core::AppError(core::ErrorCode::BatchNotFound, "Stock record for this batch does not exist.")
            );
        }

        int currentQty = q.value(0).toInt();
        int diff = physicalCount - currentQty;
        if (diff == 0) {
            return core::Result<void, core::AppError>::ok(); // Nothing to change
        }

        domain::MovementType mType = (diff > 0) 
            ? domain::MovementType::StockCorrectionAdd 
            : domain::MovementType::StockCorrectionSub;

        return applyMovement(itemId, batchId, mType, diff, reason, 0, userId, db);
    });
}

core::Result<void, core::AppError> StockService::applyMovement(
    int itemId,
    int batchId,
    domain::MovementType type,
    int quantityDelta,
    const QString& reason,
    int referenceId,
    int userId,
    QSqlDatabase& db)
{
    // Update or insert into stock_balances
    QSqlQuery q(db);
    q.prepare("SELECT quantity FROM stock_balances WHERE item_id = ? AND batch_id = ?");
    q.addBindValue(itemId);
    q.addBindValue(batchId);
    if (!q.exec()) {
        return core::Result<void, core::AppError>::err(core::AppError::fromSqlError(q.lastError().text()));
    }

    int currentQty = 0;
    bool exists = q.next();
    if (exists) {
        currentQty = q.value(0).toInt();
    }

    int newQty = currentQty + quantityDelta;
    if (newQty < 0) {
        return core::Result<void, core::AppError>::err(core::AppError(core::ErrorCode::InsufficientStock));
    }

    if (exists) {
        q.prepare("UPDATE stock_balances SET quantity = ? WHERE item_id = ? AND batch_id = ?");
        q.addBindValue(newQty);
        q.addBindValue(itemId);
        q.addBindValue(batchId);
    } else {
        q.prepare("INSERT INTO stock_balances (item_id, batch_id, quantity) VALUES (?, ?, ?)");
        q.addBindValue(itemId);
        q.addBindValue(batchId);
        q.addBindValue(newQty);
    }

    if (!q.exec()) {
        return core::Result<void, core::AppError>::err(core::AppError::fromSqlError(q.lastError().text()));
    }

    // Append to immutable stock_movements ledger
    QString typeStr;
    switch (type) {
    case domain::MovementType::Purchase: typeStr = "Purchase"; break;
    case domain::MovementType::Sale: typeStr = "Sale"; break;
    case domain::MovementType::SaleReturn: typeStr = "SaleReturn"; break;
    case domain::MovementType::PurchaseReturn: typeStr = "PurchaseReturn"; break;
    case domain::MovementType::OpeningStock: typeStr = "OpeningStock"; break;
    case domain::MovementType::StockCorrectionAdd: typeStr = "CorrectionAdd"; break;
    case domain::MovementType::StockCorrectionSub: typeStr = "CorrectionSub"; break;
    case domain::MovementType::DamagedStock: typeStr = "Damaged"; break;
    case domain::MovementType::ExpiredStock: typeStr = "Expired"; break;
    }

    q.prepare(R"(
        INSERT INTO stock_movements (item_id, batch_id, movement_type, quantity_delta, balance_after, reason, reference_id, user_id)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?)
    )");
    q.addBindValue(itemId);
    q.addBindValue(batchId);
    q.addBindValue(typeStr);
    q.addBindValue(quantityDelta);
    q.addBindValue(newQty);
    q.addBindValue(reason);
    q.addBindValue(referenceId);
    q.addBindValue(userId);

    if (!q.exec()) {
        return core::Result<void, core::AppError>::err(core::AppError::fromSqlError(q.lastError().text()));
    }

    return core::Result<void, core::AppError>::ok();
}

core::Result<std::vector<domain::StockItemView>, core::AppError> StockService::getExpiringItems(int withinDays)
{
    auto& dbMgr = database::DatabaseManager::instance();
    QSqlDatabase db = dbMgr.connection();
    if (!db.isOpen()) {
        return core::Result<std::vector<domain::StockItemView>, core::AppError>::err(
            core::AppError(core::ErrorCode::DatabaseConnectionLost)
        );
    }

    QDate limitDate = QDate::currentDate().addDays(withinDays);
    QSqlQuery q(db);
    q.prepare(R"(
        SELECT 
            i.id, i.code, i.name, c.name AS category_name,
            sb.quantity AS batch_qty,
            b.batch_number, b.expiry_date,
            i.sale_price_paisa
        FROM batches b
        JOIN items i ON b.item_id = i.id
        LEFT JOIN categories c ON i.category_id = c.id
        JOIN stock_balances sb ON b.id = sb.batch_id AND sb.item_id = i.id
        WHERE sb.quantity > 0 AND b.expiry_date <= ?
        ORDER BY b.expiry_date ASC
    )");
    q.addBindValue(limitDate);

    if (!q.exec()) {
        return core::Result<std::vector<domain::StockItemView>, core::AppError>::err(
            core::AppError::fromSqlError(q.lastError().text())
        );
    }

    std::vector<domain::StockItemView> items;
    QDate today = QDate::currentDate();
    while (q.next()) {
        domain::StockItemView item;
        item.itemId = q.value("id").toInt();
        item.code = q.value("code").toString();
        item.itemName = q.value("name").toString();
        item.categoryName = q.value("category_name").toString();
        item.totalAtomicQty = q.value("batch_qty").toInt();
        item.nearestBatch = q.value("batch_number").toString();
        item.nearestExpiry = q.value("expiry_date").toDate();
        item.retailPrice = core::Money::fromPaisa(q.value("sale_price_paisa").toLongLong());

        if (*item.nearestExpiry < today) {
            item.status = domain::StockStatus::Expired;
        } else {
            item.status = domain::StockStatus::NearExpiry;
        }
        items.push_back(std::move(item));
    }

    return core::Result<std::vector<domain::StockItemView>, core::AppError>::ok(std::move(items));
}

} // namespace services
