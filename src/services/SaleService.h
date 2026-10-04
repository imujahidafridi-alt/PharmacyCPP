#pragma once
#include <vector>
#include <QString>
#include <mutex>
#include "domain/Models.h"
#include "core/Result.h"
#include "core/Errors.h"

namespace services {

class SaleService {
public:
    static SaleService& instance();

    // Bill Number generation (Thread-safe sequence: BILL-YYYYMMDD-0001)
    QString generateNextBillNumber();

    // Atomic Checkout Transaction
    core::Result<domain::Sale, core::AppError> completeSale(domain::Sale sale, int userId);

    // Cancel / Void a completed bill (SRS Section 31: soft cancel with audit reason and stock reversal)
    core::Result<void, core::AppError> cancelSale(int saleId, const QString& reason, int authorizedUserId);

    // Bill Park / Hold feature (SRS Section 37: F7 Hold, F8 Recall)
    QString holdCurrentBill(const std::vector<domain::CartItem>& items, const QString& customerName);
    std::vector<domain::HeldBill> getHeldBills();
    std::optional<domain::HeldBill> restoreHeldBill(const QString& holdId);
    void removeHeldBill(const QString& holdId);

    // Fetch sale for receipt printing or return lookup
    core::Result<domain::Sale, core::AppError> getSaleByBillNumber(const QString& billNumber);
    core::Result<domain::Sale, core::AppError> getSaleById(int saleId);

private:
    SaleService() = default;
    std::mutex m_holdMutex;
    std::vector<domain::HeldBill> m_heldBills;
};

} // namespace services
