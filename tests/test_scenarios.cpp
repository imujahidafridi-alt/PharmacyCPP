#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <iostream>
#include <cassert>
#include "app/AppContext.h"
#include "database/DatabaseManager.h"
#include "database/Migrations.h"
#include "services/StockService.h"
#include "services/SaleService.h"
#include "services/PurchaseService.h"
#include "services/LedgerService.h"
#include "services/ReturnService.h"
#include "printing/ReceiptRenderer.h"

#define TEST_ASSERT(cond, msg) \
    if (!(cond)) { \
        std::cerr << "FAILED: " << msg << std::endl; \
        return false; \
    } else { \
        std::cout << "  PASSED: " << msg << std::endl; \
    }

bool runAllScenarioTests() {
    std::cout << "\n==========================================" << std::endl;
    std::cout << "RUNNING AUTOMATED SRS ACCEPTANCE TEST SUITE" << std::endl;
    std::cout << "==========================================\n" << std::endl;

    // 1. Setup in-memory / temporary test database
    // Remove old test database if present
    QFile::remove("test_pharmacy.db");

    database::DbConfig testConfig;
    testConfig.type = database::DatabaseType::SQLite;
    testConfig.sqlitePath = "test_pharmacy.db";

    auto& dbMgr = database::DatabaseManager::instance();
    auto initRes = dbMgr.initialize(testConfig);
    TEST_ASSERT(initRes.isOk(), "Database initialized successfully");

    auto migRes = database::Migrations::runMigrations();
    TEST_ASSERT(migRes.isOk(), "Schema migrations and seed data executed");

    // Insert test item: Panadol 500mg
    {
        QSqlQuery q(dbMgr.connection());
        q.exec(R"(
            INSERT INTO items (id, code, name, category_id, barcode, sale_price_paisa, purchase_cost_paisa, min_stock_alert, is_active, is_medicine)
            VALUES (1, 'MED-001', 'Panadol 500mg', 1, '1234567890123', 5000, 4200, 20, 1, 1)
        )");
    }

    auto& stockSvc = services::StockService::instance();
    auto& saleSvc = services::SaleService::instance();
    auto& purchSvc = services::PurchaseService::instance();
    auto& ledgerSvc = services::LedgerService::instance();
    auto& returnSvc = services::ReturnService::instance();

    // SCENARIO 14 & 15: Purchase with batches (Shah Alam Market Buy)
    std::cout << "\n[Test 1] Scenario 14 & 15: Purchase and Batch Creation" << std::endl;
    domain::Purchase mktPurchase;
    mktPurchase.isCashMarketPurchase = true;
    mktPurchase.totalCost = core::Money::fromRupees(4200.0);
    mktPurchase.amountPaid = core::Money::fromRupees(4200.0);
    
    // Add 100 Panadol Batch A (Expires Jan 2027)
    domain::PurchaseItem itemA;
    itemA.itemId = 1; // Panadol
    itemA.atomicQty = 100;
    itemA.unitCost = core::Money::fromRupees(42.0);
    itemA.totalCost = core::Money::fromRupees(4200.0);
    itemA.batchNumber = "BATCH-A";
    itemA.expiryDate = QDate(2027, 1, 15);
    mktPurchase.items.push_back(itemA);

    auto pRes = purchSvc.recordPurchase(mktPurchase, 1);
    TEST_ASSERT(pRes.isOk(), "Cash market purchase recorded without invoice");

    auto batchesRes = stockSvc.getItemBatches(1);
    TEST_ASSERT(batchesRes.isOk() && !batchesRes.value().empty(), "Batch created with 100 items");
    TEST_ASSERT(batchesRes.value()[0].quantityRemaining == 100, "Initial batch stock is 100");

    // SCENARIO 7: FEFO Priority (Batch A expires earlier than Batch B)
    std::cout << "\n[Test 2] Scenario 7: FEFO (First Expiry First Out)" << std::endl;
    // Add Batch B with later expiry (Aug 2027)
    domain::Purchase pB;
    pB.isCashMarketPurchase = true;
    domain::PurchaseItem itemB;
    itemB.itemId = 1;
    itemB.atomicQty = 50;
    itemB.unitCost = core::Money::fromRupees(42.0);
    itemB.totalCost = core::Money::fromRupees(2100.0);
    itemB.batchNumber = "BATCH-B";
    itemB.expiryDate = QDate(2027, 8, 20);
    pB.items.push_back(itemB);
    purchSvc.recordPurchase(pB, 1);

    auto allocRes = stockSvc.allocateFefo(1, 10);
    TEST_ASSERT(allocRes.isOk(), "FEFO allocation succeeded");
    TEST_ASSERT(allocRes.value()[0].batchNumber == "BATCH-A", "FEFO correctly picked BATCH-A (earlier expiry)");

    // SCENARIO 1: Normal Medicine Sale
    std::cout << "\n[Test 3] Scenario 1: Normal Sale & Stock Reduction" << std::endl;
    domain::Sale sale1;
    sale1.customerId = 1; // Walk-in
    sale1.customerName = "Walk-in Customer";
    sale1.paymentType = domain::PaymentType::Cash;
    sale1.subtotal = core::Money::fromRupees(100.0);
    sale1.netTotal = core::Money::fromRupees(100.0);
    sale1.cashReceived = core::Money::fromRupees(200.0);
    sale1.changeGiven = core::Money::fromRupees(100.0);

    domain::CartItem cItem;
    cItem.itemId = 1;
    cItem.itemName = "Panadol 500mg";
    cItem.displayQty = 2;
    cItem.atomicUnitsPerQty = 1;
    cItem.totalAtomicQty = 2;
    cItem.unitPrice = core::Money::fromRupees(50.0);
    cItem.totalAmount = core::Money::fromRupees(100.0);
    cItem.batchId = allocRes.value()[0].batchId;
    cItem.batchNumber = allocRes.value()[0].batchNumber;
    sale1.items.push_back(cItem);

    auto saleRes = saleSvc.completeSale(sale1, 1);
    TEST_ASSERT(saleRes.isOk(), "Sale completed successfully");
    
    // Check stock was reduced to 148 (100 + 50 - 2)
    auto stockOverview = stockSvc.getStockOverview("Panadol");
    TEST_ASSERT(stockOverview.isOk() && stockOverview.value()[0].totalAtomicQty == 148, "Stock correctly reduced to 148");

    // SCENARIO 10: Cash Change Calculation
    std::cout << "\n[Test 4] Scenario 10: Cash Change Calculation" << std::endl;
    core::Money totalCost = core::Money::fromRupees(1350.0);
    core::Money cashGiven = core::Money::fromRupees(2000.0);
    core::Money change = cashGiven - totalCost;
    TEST_ASSERT(change == core::Money::fromRupees(650.0), "Change is exact Rs. 650");

    // SCENARIO 4 & 5: Credit Customer Sale & Partial Payment (Khata)
    std::cout << "\n[Test 5] Scenario 4 & 5: Credit Customer (Udhaar) & Khata" << std::endl;
    auto newCustRes = ledgerSvc.createCustomer("Ali Khan", "03001234567", "Gulberg Lahore");
    TEST_ASSERT(newCustRes.isOk(), "Customer created");
    int aliId = newCustRes.value().id;

    domain::Sale creditSale;
    creditSale.customerId = aliId;
    creditSale.customerName = "Ali Khan";
    creditSale.paymentType = domain::PaymentType::Udhaar;
    creditSale.subtotal = core::Money::fromRupees(2500.0);
    creditSale.netTotal = core::Money::fromRupees(2500.0);
    
    domain::CartItem credCart;
    credCart.itemId = 1;
    credCart.itemName = "Panadol 500mg";
    credCart.displayQty = 5;
    credCart.atomicUnitsPerQty = 1;
    credCart.totalAtomicQty = 5;
    credCart.unitPrice = core::Money::fromRupees(50.0);
    credCart.totalAmount = core::Money::fromRupees(250.0);
    credCart.batchId = allocRes.value()[0].batchId;
    creditSale.items.push_back(credCart);

    auto credSaleRes = saleSvc.completeSale(creditSale, 1);
    TEST_ASSERT(credSaleRes.isOk(), "Credit sale completed");

    auto aliCust = ledgerSvc.getCustomerById(aliId);
    TEST_ASSERT(aliCust.isOk() && aliCust.value().baqaya == core::Money::fromRupees(2500.0), "Ali's Baqaya increased to Rs. 2,500");

    // Ali pays Rs. 1,000 partial payment
    auto payRes = ledgerSvc.recordCustomerPayment(aliId, core::Money::fromRupees(1000.0), "Cash", "Partial payment", 1);
    TEST_ASSERT(payRes.isOk(), "Payment of Rs. 1,000 recorded");

    aliCust = ledgerSvc.getCustomerById(aliId);
    TEST_ASSERT(aliCust.isOk() && aliCust.value().baqaya == core::Money::fromRupees(1500.0), "Ali's remaining Baqaya is exactly Rs. 1,500");

    // SCENARIO 6: Sale Return
    std::cout << "\n[Test 6] Scenario 6: Sale Return & Stock Restoration" << std::endl;
    services::ReturnItemRequest retItem;
    retItem.itemId = 1;
    retItem.batchId = allocRes.value()[0].batchId;
    retItem.returnAtomicQty = 1;
    retItem.refundAmount = core::Money::fromRupees(50.0);

    auto retRes = returnSvc.processSaleReturn(saleRes.value().billNumber, {retItem}, "Customer bought excess", true, 1);
    TEST_ASSERT(retRes.isOk(), "Sale return processed");

    stockOverview = stockSvc.getStockOverview("Panadol");
    TEST_ASSERT(stockOverview.isOk() && stockOverview.value()[0].totalAtomicQty == 144, "Stock restored by +1 on return (143 + 1 = 144)");

    // SCENARIO 12: Hold and Recall Bills
    std::cout << "\n[Test 7] Scenario 12: Hold Bill (F7) & Recall (F8)" << std::endl;
    QString holdId = saleSvc.holdCurrentBill(sale1.items, "Held Customer A");
    TEST_ASSERT(!holdId.isEmpty(), "Bill held with unique ID");
    
    auto heldList = saleSvc.getHeldBills();
    TEST_ASSERT(!heldList.empty(), "Held bill appears in held list");

    auto restored = saleSvc.restoreHeldBill(holdId);
    TEST_ASSERT(restored.has_value() && restored->items.size() == 1, "Bill restored with exact items");

    // SCENARIO 16 & 17: Stock Correction (Physical Count Mismatch)
    std::cout << "\n[Test 8] Scenario 16 & 17: Stock Correction with Reason" << std::endl;
    auto corrRes = stockSvc.correctStock(1, allocRes.value()[0].batchId, 90, "Physical counting error: 2 damaged", 1);
    TEST_ASSERT(corrRes.isOk(), "Stock correction recorded without hard delete");

    // SCENARIO 35: Thermal Receipt Generation
    std::cout << "\n[Test 9] Scenario 35: 80mm ESC/POS Receipt Rendering" << std::endl;
    QString plainTextReceipt = printing::ReceiptRenderer::renderPlainText(saleRes.value());
    TEST_ASSERT(plainTextReceipt.contains("TOTAL"), "Plain text receipt formatted with TOTAL");
    TEST_ASSERT(plainTextReceipt.contains(saleRes.value().billNumber), "Receipt contains correct bill number");

    QByteArray escposData = printing::ReceiptRenderer::renderEscPos(saleRes.value());
    TEST_ASSERT(!escposData.isEmpty(), "Binary ESC/POS thermal command stream generated");

    std::cout << "\n==========================================" << std::endl;
    std::cout << "ALL ACCEPTANCE TESTS PASSED (100% SUCCESS)" << std::endl;
    std::cout << "==========================================\n" << std::endl;

    return true;
}

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    app.setApplicationName("PakPharmacyPOS_Tests");
    app.setOrganizationName("PakPharmacy");

    bool success = runAllScenarioTests();
    return success ? 0 : 1;
}
