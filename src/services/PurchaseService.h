#pragma once
#include <vector>
#include <QString>
#include "domain/Models.h"
#include "core/Result.h"
#include "core/Errors.h"

namespace services {

class PurchaseService {
public:
    static PurchaseService& instance();

    // Record a purchase (distributor invoice OR Pakistani local cash market purchase)
    core::Result<domain::Purchase, core::AppError> recordPurchase(domain::Purchase purchase, int userId);

    // Purchase return (SRS Section 24: return damaged/near-expiry stock to supplier)
    core::Result<void, core::AppError> recordPurchaseReturn(
        int supplierId,
        int itemId,
        int batchId,
        int returnQty,
        core::Money refundAmount,
        const QString& reason,
        int userId
    );

    core::Result<std::vector<domain::Purchase>, core::AppError> getRecentPurchases(int limit = 50);

private:
    PurchaseService() = default;
};

} // namespace services
