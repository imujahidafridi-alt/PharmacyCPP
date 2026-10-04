#include "services/SaleService.h"
#include "services/StockService.h"
#include "database/DatabaseManager.h"
#include "app/AppContext.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QUuid>

namespace services {

SaleService& SaleService::instance()
{
    static SaleService inst;
    return inst;
}

QString SaleService::generateNextBillNumber()
{
    auto& dbMgr = database::DatabaseManager::instance();
    QSqlDatabase db = dbMgr.connection();
    
    QString datePart = QDate::currentDate().toString("yyyyMMdd");
    int nextSeq = 1;

    if (db.isOpen()) {
        QSqlQuery q(db);
        q.prepare("SELECT COUNT(*) FROM sales WHERE bill_number LIKE ?");
        q.addBindValue(QString("BILL-%1-%").arg(datePart));
        if (q.exec() && q.next()) {
            nextSeq = q.value(0).toInt() + 1;
        }
    }

    return QString("BILL-%1-%2").arg(datePart).arg(nextSeq, 4, 10, QChar('0'));
}

core::Result<domain::Sale, core::AppError> SaleService::completeSale(domain::Sale sale, int userId)
{
    if (sale.items.empty()) {
        return core::Result<domain::Sale, core::AppError>::err(
            core::AppError(core::ErrorCode::ValidationFailed, "Sale items cannot be empty.")
        );
    }

    if (sale.billNumber.isEmpty()) {
        sale.billNumber = generateNextBillNumber();
    }
    sale.createdAt = QDateTime::currentDateTime();
    sale.userId = userId;

    auto& dbMgr = database::DatabaseManager::instance();
    int newSaleId = 0;

    auto res = dbMgr.executeTransaction([&](QSqlDatabase& db) -> core::Result<void, core::AppError> {
        QSqlQuery q(db);
        
        QString paymentTypeStr = "Cash";
        if (sale.payments.size() > 1) {
            paymentTypeStr = "Split";
        } else if (!sale.payments.empty()) {
            switch (sale.payments.front().type) {
                case domain::PaymentType::Udhaar: paymentTypeStr = "Udhaar"; break;
                case domain::PaymentType::Easypaisa: paymentTypeStr = "EasyPaisa"; break;
                case domain::PaymentType::JazzCash: paymentTypeStr = "JazzCash"; break;
                case domain::PaymentType::Card: paymentTypeStr = "Card"; break;
                case domain::PaymentType::BankTransfer: paymentTypeStr = "BankTransfer"; break;
                default: paymentTypeStr = "Cash"; break;
            }
        } else if (sale.paymentType == domain::PaymentType::Udhaar) {
            paymentTypeStr = "Udhaar";
        }

        // 1. Insert Sales Header
        q.prepare(R"(
            INSERT INTO sales (
                bill_number, customer_id, payment_type, subtotal_paisa, discount_paisa,
                net_total_paisa, cash_received_paisa, change_given_paisa, status, user_id, created_at
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, 'Completed', ?, ?)
        )");
        q.addBindValue(sale.billNumber);
        q.addBindValue(sale.customerId);
        q.addBindValue(paymentTypeStr);
        q.addBindValue(sale.subtotal.paisa());
        q.addBindValue(sale.discount.paisa());
        q.addBindValue(sale.netTotal.paisa());
        q.addBindValue(sale.cashReceived.paisa());
        q.addBindValue(sale.changeGiven.paisa());
        q.addBindValue(userId);
        q.addBindValue(sale.createdAt);

        if (!q.exec()) {
            return core::Result<void, core::AppError>::err(
                core::AppError::fromSqlError(q.lastError().text(), "Failed to record sale.")
            );
        }

        newSaleId = q.lastInsertId().toInt();
        if (newSaleId == 0) {
            // Postgres fallback to query id
            QSqlQuery idQuery(db);
            idQuery.prepare("SELECT id FROM sales WHERE bill_number = ?");
            idQuery.addBindValue(sale.billNumber);
            if (idQuery.exec() && idQuery.next()) {
                newSaleId = idQuery.value(0).toInt();
            }
        }
        sale.id = newSaleId;

        // 2. Insert Sale Items & Decrement Stock via FEFO
        for (const auto& item : sale.items) {
            QString unitTypeStr;
            switch (item.unitSelection) {
            case domain::SaleUnitSelection::Box: unitTypeStr = "Box"; break;
            case domain::SaleUnitSelection::Strip: unitTypeStr = "Strip"; break;
            default: unitTypeStr = "Piece"; break;
            }

            q.prepare(R"(
                INSERT INTO sale_items (
                    sale_id, item_id, batch_id, unit_type, display_qty, atomic_qty, unit_price_paisa, total_amount_paisa
                ) VALUES (?, ?, ?, ?, ?, ?, ?, ?)
            )");
            q.addBindValue(newSaleId);
            q.addBindValue(item.itemId);
            q.addBindValue(item.batchId > 0 ? QVariant(item.batchId) : QVariant());
            q.addBindValue(unitTypeStr);
            q.addBindValue(item.displayQty);
            q.addBindValue(item.totalAtomicQty);
            q.addBindValue(item.unitPrice.paisa());
            q.addBindValue(item.totalAmount.paisa());

            if (!q.exec()) {
                return core::Result<void, core::AppError>::err(
                    core::AppError::fromSqlError(q.lastError().text(), "Failed to record sale item.")
                );
            }

            // Apply stock reduction atomically (returns add stock automatically if totalAtomicQty < 0)
            auto stockRes = StockService::instance().applyMovement(
                item.itemId,
                item.batchId,
                domain::MovementType::Sale,
                -item.totalAtomicQty,
                QString("Sold on Bill #%1").arg(sale.billNumber),
                newSaleId,
                userId,
                db
            );

            if (stockRes.isErr()) {
                return stockRes;
            }
        }

        // 3. Process Split Payment Allocations (Cash Session, Customer Khata)
        core::Money cashPortion;
        core::Money udhaarPortion;

        if (!sale.payments.empty()) {
            for (const auto& alloc : sale.payments) {
                if (alloc.type == domain::PaymentType::Cash) {
                    cashPortion += alloc.amount;
                } else if (alloc.type == domain::PaymentType::Udhaar) {
                    udhaarPortion += alloc.amount;
                }
            }
        } else {
            if (sale.paymentType == domain::PaymentType::Udhaar) {
                udhaarPortion = sale.netTotal;
            } else {
                cashPortion = sale.netTotal;
            }
        }

        // Customer Ledger for Udhaar / Credit Sales (SRS Section 20)
        if (udhaarPortion.isPositive() && sale.customerId > 1) {
            q.prepare("SELECT baqaya_paisa FROM customers WHERE id = ?");
            q.addBindValue(sale.customerId);
            if (!q.exec() || !q.next()) {
                return core::Result<void, core::AppError>::err(
                    core::AppError(core::ErrorCode::CustomerNotFound)
                );
            }

            int64_t prevBaqaya = q.value(0).toLongLong();
            int64_t newBaqaya = prevBaqaya + udhaarPortion.paisa();

            // Update customer balance
            q.prepare("UPDATE customers SET baqaya_paisa = ? WHERE id = ?");
            q.addBindValue(newBaqaya);
            q.addBindValue(sale.customerId);
            if (!q.exec()) {
                return core::Result<void, core::AppError>::err(core::AppError::fromSqlError(q.lastError().text()));
            }

            // Ledger entry
            q.prepare(R"(
                INSERT INTO customer_ledger (customer_id, reference_type, reference_id, debit_paisa, credit_paisa, balance_after_paisa, description, user_id)
                VALUES (?, 'Udhaar Sale', ?, ?, 0, ?, ?, ?)
            )");
            q.addBindValue(sale.customerId);
            q.addBindValue(newSaleId);
            q.addBindValue(udhaarPortion.paisa());
            q.addBindValue(newBaqaya);
            q.addBindValue(QString("Bill #%1 (Khata Portion)").arg(sale.billNumber));
            q.addBindValue(userId);
            if (!q.exec()) {
                return core::Result<void, core::AppError>::err(core::AppError::fromSqlError(q.lastError().text()));
            }
        }

        // 4. Update Cash Session if active
        auto sessionOpt = app::AppContext::instance().activeSession();
        if (sessionOpt.has_value() && cashPortion.isPositive()) {
            q.prepare("UPDATE cash_sessions SET cash_sales_paisa = cash_sales_paisa + ? WHERE id = ? AND is_open = 1");
            q.addBindValue(cashPortion.paisa());
            q.addBindValue(sessionOpt->id);
            q.exec();
        }

        // 5. Audit Log Entry
        q.prepare(R"(
            INSERT INTO audit_logs (user_id, action, entity, entity_id, new_value, reason)
            VALUES (?, 'CREATE_SALE', 'sales', ?, ?, 'Sale completed at counter')
        )");
        q.addBindValue(userId);
        q.addBindValue(newSaleId);
        q.addBindValue(QString("Bill #%1 Total: %2").arg(sale.billNumber, sale.netTotal.formatted()));
        q.exec();

        return core::Result<void, core::AppError>::ok();
    });

    if (res.isErr()) {
        return core::Result<domain::Sale, core::AppError>::err(res.error());
    }

    return core::Result<domain::Sale, core::AppError>::ok(sale);
}

core::Result<void, core::AppError> SaleService::cancelSale(int saleId, const QString& reason, int authorizedUserId)
{
    if (reason.trimmed().isEmpty()) {
        return core::Result<void, core::AppError>::err(
            core::AppError(core::ErrorCode::ValidationFailed, "Cancellation reason is mandatory.")
        );
    }

    auto& dbMgr = database::DatabaseManager::instance();
    return dbMgr.executeTransaction([&](QSqlDatabase& db) -> core::Result<void, core::AppError> {
        QSqlQuery q(db);
        q.prepare("SELECT bill_number, customer_id, payment_type, net_total_paisa, status FROM sales WHERE id = ?");
        q.addBindValue(saleId);
        if (!q.exec() || !q.next()) {
            return core::Result<void, core::AppError>::err(core::AppError(core::ErrorCode::SaleNotFound));
        }

        QString status = q.value("status").toString();
        if (status == "Cancelled") {
            return core::Result<void, core::AppError>::err(
                core::AppError(core::ErrorCode::ValidationFailed, "Bill is already cancelled.")
            );
        }

        QString billNumber = q.value("bill_number").toString();
        int customerId = q.value("customer_id").toInt();
        QString paymentType = q.value("payment_type").toString();
        int64_t netTotalPaisa = q.value("net_total_paisa").toLongLong();

        // Query sale items to reverse stock
        q.prepare("SELECT item_id, batch_id, atomic_qty FROM sale_items WHERE sale_id = ?");
        q.addBindValue(saleId);
        if (!q.exec()) {
            return core::Result<void, core::AppError>::err(core::AppError::fromSqlError(q.lastError().text()));
        }

        struct ReversalItem { int itemId; int batchId; int qty; };
        std::vector<ReversalItem> reversals;
        while (q.next()) {
            reversals.push_back({q.value("item_id").toInt(), q.value("batch_id").toInt(), q.value("atomic_qty").toInt()});
        }

        for (const auto& rev : reversals) {
            auto stockRes = StockService::instance().applyMovement(
                rev.itemId,
                rev.batchId,
                domain::MovementType::SaleReturn,
                rev.qty,
                QString("Cancelled Bill #%1: %2").arg(billNumber, reason),
                saleId,
                authorizedUserId,
                db
            );
            if (stockRes.isErr()) return stockRes;
        }

        // Adjust Udhaar if credit sale
        if (paymentType == "Udhaar" && customerId > 1) {
            q.prepare("UPDATE customers SET baqaya_paisa = baqaya_paisa - ? WHERE id = ?");
            q.addBindValue(netTotalPaisa);
            q.addBindValue(customerId);
            q.exec();

            q.prepare(R"(
                INSERT INTO customer_ledger (customer_id, reference_type, reference_id, debit_paisa, credit_paisa, balance_after_paisa, description, user_id)
                VALUES (?, 'Bill Cancellation', ?, 0, ?, (SELECT baqaya_paisa FROM customers WHERE id = ?), ?, ?)
            )");
            q.addBindValue(customerId);
            q.addBindValue(saleId);
            q.addBindValue(netTotalPaisa);
            q.addBindValue(customerId);
            q.addBindValue(QString("Cancelled Bill #%1: %2").arg(billNumber, reason));
            q.addBindValue(authorizedUserId);
            q.exec();
        }

        // Mark sale as cancelled (Soft cancel per SRS Section 31 and 62)
        q.prepare("UPDATE sales SET status = 'Cancelled', cancel_reason = ? WHERE id = ?");
        q.addBindValue(reason);
        q.addBindValue(saleId);
        if (!q.exec()) {
            return core::Result<void, core::AppError>::err(core::AppError::fromSqlError(q.lastError().text()));
        }

        // Audit Log
        q.prepare("INSERT INTO audit_logs (user_id, action, entity, entity_id, new_value, reason) VALUES (?, 'CANCEL_SALE', 'sales', ?, 'Cancelled', ?)");
        q.addBindValue(authorizedUserId);
        q.addBindValue(saleId);
        q.addBindValue(reason);
        q.exec();

        return core::Result<void, core::AppError>::ok();
    });
}

QString SaleService::holdCurrentBill(const std::vector<domain::CartItem>& items, const QString& customerName)
{
    std::lock_guard<std::mutex> lock(m_holdMutex);
    QString holdId = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
    
    domain::HeldBill bill;
    bill.holdId = holdId;
    bill.customerName = customerName.isEmpty() ? "Walk-in Customer" : customerName;
    bill.holdTime = QDateTime::currentDateTime();
    bill.items = items;
    
    core::Money tot;
    for (const auto& it : items) {
        tot += it.totalAmount;
    }
    bill.total = tot;

    m_heldBills.push_back(std::move(bill));
    return holdId;
}

std::vector<domain::HeldBill> SaleService::getHeldBills()
{
    std::lock_guard<std::mutex> lock(m_holdMutex);
    return m_heldBills;
}

std::optional<domain::HeldBill> SaleService::restoreHeldBill(const QString& holdId)
{
    std::lock_guard<std::mutex> lock(m_holdMutex);
    for (auto it = m_heldBills.begin(); it != m_heldBills.end(); ++it) {
        if (it->holdId == holdId) {
            domain::HeldBill found = *it;
            m_heldBills.erase(it);
            return found;
        }
    }
    return std::nullopt;
}

void SaleService::removeHeldBill(const QString& holdId)
{
    std::lock_guard<std::mutex> lock(m_holdMutex);
    for (auto it = m_heldBills.begin(); it != m_heldBills.end(); ++it) {
        if (it->holdId == holdId) {
            m_heldBills.erase(it);
            break;
        }
    }
}

core::Result<domain::Sale, core::AppError> SaleService::getSaleByBillNumber(const QString& billNumber)
{
    auto& dbMgr = database::DatabaseManager::instance();
    QSqlDatabase db = dbMgr.connection();
    if (!db.isOpen()) {
        return core::Result<domain::Sale, core::AppError>::err(core::AppError(core::ErrorCode::DatabaseConnectionLost));
    }

    QSqlQuery q(db);
    q.prepare(R"(
        SELECT s.*, c.name AS customer_name, u.full_name AS cashier_name
        FROM sales s
        LEFT JOIN customers c ON s.customer_id = c.id
        LEFT JOIN users u ON s.user_id = u.id
        WHERE s.bill_number = ?
    )");
    q.addBindValue(billNumber);

    if (!q.exec() || !q.next()) {
        return core::Result<domain::Sale, core::AppError>::err(core::AppError(core::ErrorCode::SaleNotFound));
    }

    domain::Sale sale;
    sale.id = q.value("id").toInt();
    sale.billNumber = q.value("bill_number").toString();
    sale.customerId = q.value("customer_id").toInt();
    sale.customerName = q.value("customer_name").toString();
    sale.paymentType = (q.value("payment_type").toString() == "Udhaar") ? domain::PaymentType::Udhaar : domain::PaymentType::Cash;
    sale.subtotal = core::Money::fromPaisa(q.value("subtotal_paisa").toLongLong());
    sale.discount = core::Money::fromPaisa(q.value("discount_paisa").toLongLong());
    sale.netTotal = core::Money::fromPaisa(q.value("net_total_paisa").toLongLong());
    sale.cashReceived = core::Money::fromPaisa(q.value("cash_received_paisa").toLongLong());
    sale.changeGiven = core::Money::fromPaisa(q.value("change_given_paisa").toLongLong());
    sale.createdAt = q.value("created_at").toDateTime();
    sale.cashierName = q.value("cashier_name").toString();

    // Query items
    QSqlQuery itemQ(db);
    itemQ.prepare(R"(
        SELECT si.*, i.name AS item_name, b.batch_number, b.expiry_date
        FROM sale_items si
        JOIN items i ON si.item_id = i.id
        LEFT JOIN batches b ON si.batch_id = b.id
        WHERE si.sale_id = ?
    )");
    itemQ.addBindValue(sale.id);
    if (itemQ.exec()) {
        while (itemQ.next()) {
            domain::CartItem it;
            it.itemId = itemQ.value("item_id").toInt();
            it.itemName = itemQ.value("item_name").toString();
            it.displayQty = itemQ.value("display_qty").toInt();
            it.totalAtomicQty = itemQ.value("atomic_qty").toInt();
            it.unitPrice = core::Money::fromPaisa(itemQ.value("unit_price_paisa").toLongLong());
            it.totalAmount = core::Money::fromPaisa(itemQ.value("total_amount_paisa").toLongLong());
            it.batchId = itemQ.value("batch_id").toInt();
            it.batchNumber = itemQ.value("batch_number").toString();
            it.expiryDate = itemQ.value("expiry_date").toDate();
            sale.items.push_back(std::move(it));
        }
    }

    return core::Result<domain::Sale, core::AppError>::ok(std::move(sale));
}

core::Result<domain::Sale, core::AppError> SaleService::getSaleById(int saleId)
{
    auto& dbMgr = database::DatabaseManager::instance();
    QSqlDatabase db = dbMgr.connection();
    if (!db.isOpen()) {
        return core::Result<domain::Sale, core::AppError>::err(core::AppError(core::ErrorCode::DatabaseConnectionLost));
    }

    QSqlQuery q(db);
    q.prepare("SELECT bill_number FROM sales WHERE id = ?");
    q.addBindValue(saleId);
    if (!q.exec() || !q.next()) {
        return core::Result<domain::Sale, core::AppError>::err(core::AppError(core::ErrorCode::SaleNotFound));
    }

    return getSaleByBillNumber(q.value(0).toString());
}

} // namespace services
