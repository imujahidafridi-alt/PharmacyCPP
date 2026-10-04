#include "services/LedgerService.h"
#include "database/DatabaseManager.h"
#include <QSqlQuery>
#include <QSqlError>

namespace services {

LedgerService& LedgerService::instance()
{
    static LedgerService inst;
    return inst;
}

core::Result<std::vector<domain::Customer>, core::AppError> LedgerService::searchCustomers(const QString& query)
{
    auto& dbMgr = database::DatabaseManager::instance();
    QSqlDatabase db = dbMgr.connection();
    if (!db.isOpen()) {
        return core::Result<std::vector<domain::Customer>, core::AppError>::err(core::AppError(core::ErrorCode::DatabaseConnectionLost));
    }

    QString sql = "SELECT * FROM customers";
    QVariantList binds;
    if (!query.trimmed().isEmpty()) {
        sql += " WHERE name LIKE ? OR phone LIKE ?";
        QString w = "%" + query.trimmed() + "%";
        binds << w << w;
    }
    sql += " ORDER BY name ASC";

    QSqlQuery q(db);
    q.prepare(sql);
    for (const auto& b : binds) q.addBindValue(b);

    if (!q.exec()) {
        return core::Result<std::vector<domain::Customer>, core::AppError>::err(core::AppError::fromSqlError(q.lastError().text()));
    }

    std::vector<domain::Customer> customers;
    while (q.next()) {
        domain::Customer c;
        c.id = q.value("id").toInt();
        c.name = q.value("name").toString();
        c.phone = q.value("phone").toString();
        c.address = q.value("address").toString();
        c.baqaya = core::Money::fromPaisa(q.value("baqaya_paisa").toLongLong());
        c.notes = q.value("notes").toString();
        customers.push_back(std::move(c));
    }

    return core::Result<std::vector<domain::Customer>, core::AppError>::ok(std::move(customers));
}

core::Result<domain::Customer, core::AppError> LedgerService::getCustomerById(int customerId)
{
    auto& dbMgr = database::DatabaseManager::instance();
    QSqlDatabase db = dbMgr.connection();
    if (!db.isOpen()) {
        return core::Result<domain::Customer, core::AppError>::err(core::AppError(core::ErrorCode::DatabaseConnectionLost));
    }

    QSqlQuery q(db);
    q.prepare("SELECT * FROM customers WHERE id = ?");
    q.addBindValue(customerId);
    if (!q.exec() || !q.next()) {
        return core::Result<domain::Customer, core::AppError>::err(core::AppError(core::ErrorCode::CustomerNotFound));
    }

    domain::Customer c;
    c.id = q.value("id").toInt();
    c.name = q.value("name").toString();
    c.phone = q.value("phone").toString();
    c.address = q.value("address").toString();
    c.baqaya = core::Money::fromPaisa(q.value("baqaya_paisa").toLongLong());
    c.notes = q.value("notes").toString();
    return core::Result<domain::Customer, core::AppError>::ok(c);
}

core::Result<domain::Customer, core::AppError> LedgerService::createCustomer(const QString& name, const QString& phone, const QString& address)
{
    if (name.trimmed().isEmpty()) {
        return core::Result<domain::Customer, core::AppError>::err(core::AppError(core::ErrorCode::ValidationFailed, "Customer name is required."));
    }

    auto& dbMgr = database::DatabaseManager::instance();
    QSqlDatabase db = dbMgr.connection();
    if (!db.isOpen()) {
        return core::Result<domain::Customer, core::AppError>::err(core::AppError(core::ErrorCode::DatabaseConnectionLost));
    }

    QSqlQuery q(db);
    q.prepare("INSERT INTO customers (name, phone, address, baqaya_paisa) VALUES (?, ?, ?, 0)");
    q.addBindValue(name.trimmed());
    q.addBindValue(phone.trimmed());
    q.addBindValue(address.trimmed());
    if (!q.exec()) {
        return core::Result<domain::Customer, core::AppError>::err(core::AppError::fromSqlError(q.lastError().text()));
    }

    int newId = q.lastInsertId().toInt();
    return getCustomerById(newId);
}

core::Result<void, core::AppError> LedgerService::recordCustomerPayment(
    int customerId,
    core::Money amount,
    const QString& paymentMethod,
    const QString& notes,
    int userId)
{
    if (amount <= core::Money(0)) {
        return core::Result<void, core::AppError>::err(core::AppError(core::ErrorCode::InvalidPaymentAmount, "Payment amount must be greater than zero."));
    }

    auto& dbMgr = database::DatabaseManager::instance();
    return dbMgr.executeTransaction([&](QSqlDatabase& db) -> core::Result<void, core::AppError> {
        QSqlQuery q(db);
        q.prepare("SELECT baqaya_paisa FROM customers WHERE id = ?");
        q.addBindValue(customerId);
        if (!q.exec() || !q.next()) {
            return core::Result<void, core::AppError>::err(core::AppError(core::ErrorCode::CustomerNotFound));
        }

        int64_t currentBaqaya = q.value(0).toLongLong();
        int64_t newBaqaya = currentBaqaya - amount.paisa();

        // Update customer baqaya
        q.prepare("UPDATE customers SET baqaya_paisa = ? WHERE id = ?");
        q.addBindValue(newBaqaya);
        q.addBindValue(customerId);
        if (!q.exec()) {
            return core::Result<void, core::AppError>::err(core::AppError::fromSqlError(q.lastError().text()));
        }

        // Ledger credit entry (Jama)
        q.prepare(R"(
            INSERT INTO customer_ledger (customer_id, reference_type, debit_paisa, credit_paisa, balance_after_paisa, description, user_id)
            VALUES (?, 'Payment Received', 0, ?, ?, ?, ?)
        )");
        q.addBindValue(customerId);
        q.addBindValue(amount.paisa());
        q.addBindValue(newBaqaya);
        q.addBindValue(QString("%1: %2").arg(paymentMethod, notes));
        q.addBindValue(userId);
        if (!q.exec()) {
            return core::Result<void, core::AppError>::err(core::AppError::fromSqlError(q.lastError().text()));
        }

        return core::Result<void, core::AppError>::ok();
    });
}

core::Result<std::vector<LedgerEntry>, core::AppError> LedgerService::getCustomerLedger(int customerId, int limit)
{
    auto& dbMgr = database::DatabaseManager::instance();
    QSqlDatabase db = dbMgr.connection();
    if (!db.isOpen()) {
        return core::Result<std::vector<LedgerEntry>, core::AppError>::err(core::AppError(core::ErrorCode::DatabaseConnectionLost));
    }

    QSqlQuery q(db);
    q.prepare(R"(
        SELECT * FROM customer_ledger
        WHERE customer_id = ?
        ORDER BY created_at DESC
        LIMIT ?
    )");
    q.addBindValue(customerId);
    q.addBindValue(limit);

    if (!q.exec()) {
        return core::Result<std::vector<LedgerEntry>, core::AppError>::err(core::AppError::fromSqlError(q.lastError().text()));
    }

    std::vector<LedgerEntry> entries;
    while (q.next()) {
        LedgerEntry e;
        e.id = q.value("id").toInt();
        e.timestamp = q.value("created_at").toDateTime();
        e.referenceType = q.value("reference_type").toString();
        e.referenceId = q.value("reference_id").toInt();
        e.debit = core::Money::fromPaisa(q.value("debit_paisa").toLongLong());
        e.credit = core::Money::fromPaisa(q.value("credit_paisa").toLongLong());
        e.balanceAfter = core::Money::fromPaisa(q.value("balance_after_paisa").toLongLong());
        e.description = q.value("description").toString();
        entries.push_back(std::move(e));
    }

    return core::Result<std::vector<LedgerEntry>, core::AppError>::ok(std::move(entries));
}

core::Result<std::vector<domain::Supplier>, core::AppError> LedgerService::searchSuppliers(const QString& query)
{
    auto& dbMgr = database::DatabaseManager::instance();
    QSqlDatabase db = dbMgr.connection();
    if (!db.isOpen()) {
        return core::Result<std::vector<domain::Supplier>, core::AppError>::err(core::AppError(core::ErrorCode::DatabaseConnectionLost));
    }

    QString sql = "SELECT * FROM suppliers";
    QVariantList binds;
    if (!query.trimmed().isEmpty()) {
        sql += " WHERE name LIKE ? OR phone LIKE ?";
        QString w = "%" + query.trimmed() + "%";
        binds << w << w;
    }
    sql += " ORDER BY name ASC";

    QSqlQuery q(db);
    q.prepare(sql);
    for (const auto& b : binds) q.addBindValue(b);

    if (!q.exec()) {
        return core::Result<std::vector<domain::Supplier>, core::AppError>::err(core::AppError::fromSqlError(q.lastError().text()));
    }

    std::vector<domain::Supplier> suppliers;
    while (q.next()) {
        domain::Supplier s;
        s.id = q.value("id").toInt();
        s.name = q.value("name").toString();
        s.phone = q.value("phone").toString();
        s.address = q.value("address").toString();
        s.baqaya = core::Money::fromPaisa(q.value("baqaya_paisa").toLongLong());
        s.notes = q.value("notes").toString();
        suppliers.push_back(std::move(s));
    }

    return core::Result<std::vector<domain::Supplier>, core::AppError>::ok(std::move(suppliers));
}

core::Result<domain::Supplier, core::AppError> LedgerService::createSupplier(const QString& name, const QString& phone, const QString& address)
{
    if (name.trimmed().isEmpty()) {
        return core::Result<domain::Supplier, core::AppError>::err(core::AppError(core::ErrorCode::ValidationFailed, "Supplier name is required."));
    }

    auto& dbMgr = database::DatabaseManager::instance();
    QSqlDatabase db = dbMgr.connection();
    if (!db.isOpen()) {
        return core::Result<domain::Supplier, core::AppError>::err(core::AppError(core::ErrorCode::DatabaseConnectionLost));
    }

    QSqlQuery q(db);
    q.prepare("INSERT INTO suppliers (name, phone, address, baqaya_paisa) VALUES (?, ?, ?, 0)");
    q.addBindValue(name.trimmed());
    q.addBindValue(phone.trimmed());
    q.addBindValue(address.trimmed());
    if (!q.exec()) {
        return core::Result<domain::Supplier, core::AppError>::err(core::AppError::fromSqlError(q.lastError().text()));
    }

    int newId = q.lastInsertId().toInt();
    domain::Supplier s;
    s.id = newId;
    s.name = name.trimmed();
    s.phone = phone.trimmed();
    s.address = address.trimmed();
    return core::Result<domain::Supplier, core::AppError>::ok(s);
}

core::Result<void, core::AppError> LedgerService::recordSupplierPayment(
    int supplierId,
    core::Money amount,
    const QString& notes,
    int userId)
{
    if (amount <= core::Money(0)) {
        return core::Result<void, core::AppError>::err(core::AppError(core::ErrorCode::InvalidPaymentAmount));
    }

    auto& dbMgr = database::DatabaseManager::instance();
    return dbMgr.executeTransaction([&](QSqlDatabase& db) -> core::Result<void, core::AppError> {
        QSqlQuery q(db);
        q.prepare("SELECT baqaya_paisa FROM suppliers WHERE id = ?");
        q.addBindValue(supplierId);
        if (!q.exec() || !q.next()) {
            return core::Result<void, core::AppError>::err(core::AppError(core::ErrorCode::SupplierNotFound));
        }

        int64_t currentBaqaya = q.value(0).toLongLong();
        int64_t newBaqaya = currentBaqaya - amount.paisa();

        q.prepare("UPDATE suppliers SET baqaya_paisa = ? WHERE id = ?");
        q.addBindValue(newBaqaya);
        q.addBindValue(supplierId);
        if (!q.exec()) return core::Result<void, core::AppError>::err(core::AppError::fromSqlError(q.lastError().text()));

        q.prepare(R"(
            INSERT INTO supplier_ledger (supplier_id, reference_type, debit_paisa, credit_paisa, balance_after_paisa, description, user_id)
            VALUES (?, 'Payment Made', ?, 0, ?, ?, ?)
        )");
        q.addBindValue(supplierId);
        q.addBindValue(amount.paisa());
        q.addBindValue(newBaqaya);
        q.addBindValue(notes);
        q.addBindValue(userId);
        if (!q.exec()) return core::Result<void, core::AppError>::err(core::AppError::fromSqlError(q.lastError().text()));

        return core::Result<void, core::AppError>::ok();
    });
}

core::Result<std::vector<LedgerEntry>, core::AppError> LedgerService::getSupplierLedger(int supplierId, int limit)
{
    auto& dbMgr = database::DatabaseManager::instance();
    QSqlDatabase db = dbMgr.connection();
    if (!db.isOpen()) {
        return core::Result<std::vector<LedgerEntry>, core::AppError>::err(core::AppError(core::ErrorCode::DatabaseConnectionLost));
    }

    QSqlQuery q(db);
    q.prepare("SELECT * FROM supplier_ledger WHERE supplier_id = ? ORDER BY created_at DESC LIMIT ?");
    q.addBindValue(supplierId);
    q.addBindValue(limit);

    if (!q.exec()) {
        return core::Result<std::vector<LedgerEntry>, core::AppError>::err(core::AppError::fromSqlError(q.lastError().text()));
    }

    std::vector<LedgerEntry> entries;
    while (q.next()) {
        LedgerEntry e;
        e.id = q.value("id").toInt();
        e.timestamp = q.value("created_at").toDateTime();
        e.referenceType = q.value("reference_type").toString();
        e.referenceId = q.value("reference_id").toInt();
        e.debit = core::Money::fromPaisa(q.value("debit_paisa").toLongLong());
        e.credit = core::Money::fromPaisa(q.value("credit_paisa").toLongLong());
        e.balanceAfter = core::Money::fromPaisa(q.value("balance_after_paisa").toLongLong());
        e.description = q.value("description").toString();
        entries.push_back(std::move(e));
    }

    return core::Result<std::vector<LedgerEntry>, core::AppError>::ok(std::move(entries));
}

} // namespace services
