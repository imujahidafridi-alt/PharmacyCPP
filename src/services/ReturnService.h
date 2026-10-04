#pragma once
#include <vector>
#include <QString>
#include "domain/Models.h"
#include "core/Result.h"
#include "core/Errors.h"

namespace services {

struct ReturnItemRequest {
    int itemId{0};
    int batchId{0};
    int returnAtomicQty{0};
    core::Money refundAmount;
};

class ReturnService {
public:
    static ReturnService& instance();

    // Process Sale Return against original bill
    core::Result<void, core::AppError> processSaleReturn(
        const QString& billNumber,
        const std::vector<ReturnItemRequest>& returnItems,
        const QString& reason,
        bool refundInCash,
        int userId
    );

private:
    ReturnService() = default;
};

} // namespace services
