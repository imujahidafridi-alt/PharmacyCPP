#pragma once
#include <vector>
#include <QString>
#include "domain/Models.h"
#include "core/Result.h"
#include "core/Errors.h"

namespace services {

struct LedgerEntry {
    int id{0};
    QDateTime timestamp;
    QString referenceType;
    int referenceId{0};
    core::Money debit;
    core::Money credit;
    core::Money balanceAfter;
    QString description;
};

class LedgerService {
public:
    static LedgerService& instance();

    // Customer operations
    core::Result<std::vector<domain::Customer>, core::AppError> searchCustomers(const QString& query);
    core::Result<domain::Customer, core::AppError> getCustomerById(int customerId);
    core::Result<domain::Customer, core::AppError> createCustomer(const QString& name, const QString& phone, const QString& address);
    
    // Khata & Payment collection (SRS Section 21, 22)
    core::Result<void, core::AppError> recordCustomerPayment(
        int customerId,
        core::Money amount,
        const QString& paymentMethod,
        const QString& notes,
        int userId
    );

    core::Result<std::vector<LedgerEntry>, core::AppError> getCustomerLedger(int customerId, int limit = 50);

    // Supplier operations
    core::Result<std::vector<domain::Supplier>, core::AppError> searchSuppliers(const QString& query);
    core::Result<domain::Supplier, core::AppError> createSupplier(const QString& name, const QString& phone, const QString& address);
    core::Result<void, core::AppError> recordSupplierPayment(
        int supplierId,
        core::Money amount,
        const QString& notes,
        int userId
    );
    core::Result<std::vector<LedgerEntry>, core::AppError> getSupplierLedger(int supplierId, int limit = 50);

private:
    LedgerService() = default;
};

} // namespace services
