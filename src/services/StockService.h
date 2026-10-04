#pragma once
#include <vector>
#include <optional>
#include <QDate>
#include <QSqlDatabase>
#include "domain/Models.h"
#include "core/Result.h"
#include "core/Errors.h"

namespace services {

struct FefoAllocation {
    int batchId{0};
    QString batchNumber;
    QDate expiryDate;
    int allocatedQty{0};
    core::Money costPrice;
    core::Money salePrice;
    bool isExpired{false};
};

class StockService {
public:
    static StockService& instance();

    // Query stock status
    core::Result<std::vector<domain::StockItemView>, core::AppError> getStockOverview(
        const QString& searchFilter = QString(),
        int categoryFilter = 0
    );

    core::Result<std::vector<domain::Batch>, core::AppError> getItemBatches(int itemId);

    // FEFO allocation for POS cart: automatically allocates quantities from earliest expiring batch
    core::Result<std::vector<FefoAllocation>, core::AppError> allocateFefo(
        int itemId,
        int requestedAtomicQty,
        bool allowExpired = false
    );

    // Stock Correction (SRS Section 25: Physical count mismatch, damage, counting error)
    core::Result<void, core::AppError> correctStock(
        int itemId,
        int batchId,
        int physicalCount,
        const QString& reason,
        int userId
    );

    // Expiry screen queries
    core::Result<std::vector<domain::StockItemView>, core::AppError> getExpiringItems(int withinDays);

    // Adjust stock in an existing transaction (called by SaleService / PurchaseService)
    core::Result<void, core::AppError> applyMovement(
        int itemId,
        int batchId,
        domain::MovementType type,
        int quantityDelta,
        const QString& reason,
        int referenceId,
        int userId,
        QSqlDatabase& db
    );

private:
    StockService() = default;
};

} // namespace services
