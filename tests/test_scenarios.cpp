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
#include "services/PriceCalculator.h"
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

    // SCENARIO 36: Pakistani Retail Pricing Architecture & Edge Cases
    std::cout << "\n[Test 10] Pakistani Retail Pricing Engine & Loss Prevention" << std::endl;

    // 10.1 FMCG Non-Discountable Lock (Baby Milk / Pampers / Face Wash)
    domain::Item fmcgItem;
    fmcgItem.id = 101;
    fmcgItem.name = "Pampers Baby Dry Diapers";
    fmcgItem.salePrice = core::Money::fromRupees(2500.0);
    fmcgItem.purchaseCost = core::Money::fromRupees(2300.0);
    fmcgItem.tp = fmcgItem.purchaseCost;
    fmcgItem.categoryDiscountable = false; // Inherited from Baby Care category
    fmcgItem.categoryDefaultDiscountPct = 0.0;
    fmcgItem.categoryMaxDiscountPct = 0.0;

    auto fmcgComp = services::PriceCalculator::calculateRowTotals(fmcgItem, 1, 15.0);
    TEST_ASSERT(!fmcgComp.isDiscountable, "FMCG item is flagged non-discountable");
    TEST_ASSERT(fmcgComp.discountPct == 0.0, "FMCG discount % rigidly locked at 0% despite 15% request");
    TEST_ASSERT(fmcgComp.unitDiscount.paisa() == 0, "FMCG discount amount is 0 paisa");
    TEST_ASSERT(fmcgComp.unitSalePrice == core::Money::fromRupees(2500.0), "FMCG net price is printed MRP (Rs. 2,500)");
    TEST_ASSERT(fmcgComp.totalAmount == core::Money::fromRupees(2500.0), "FMCG line total equals gross MRP");

    // 10.2 Category-Level vs Item-Level Defaults
    domain::Item pharmaItem;
    pharmaItem.id = 102;
    pharmaItem.name = "Panadol 500mg Tablets";
    pharmaItem.salePrice = core::Money::fromRupees(500.0);
    pharmaItem.purchaseCost = core::Money::fromRupees(400.0);
    pharmaItem.tp = pharmaItem.purchaseCost;
    pharmaItem.categoryDiscountable = true;
    pharmaItem.categoryDefaultDiscountPct = 10.0;
    pharmaItem.categoryMaxDiscountPct = 15.0;

    // Auto inherit category default (10%)
    auto pharmaComp = services::PriceCalculator::calculateRowTotals(pharmaItem, 1, -1.0);
    TEST_ASSERT(pharmaComp.isDiscountable, "Pharma medicine is discountable");
    TEST_ASSERT(pharmaComp.discountPct == 10.0, "Pharma correctly inherits category default 10% discount");
    TEST_ASSERT(pharmaComp.unitDiscount == core::Money::fromRupees(50.0), "10% discount on Rs. 500 is exact Rs. 50");
    TEST_ASSERT(pharmaComp.unitSalePrice == core::Money::fromRupees(450.0), "Net price after 10% discount is Rs. 450");

    // Item-level override takes precedence over category
    pharmaItem.discountPctOverride = 7.0;
    auto overrideComp = services::PriceCalculator::calculateRowTotals(pharmaItem, 1, -1.0);
    TEST_ASSERT(overrideComp.discountPct == 7.0, "Item-level discount override (7%) overrides category default (10%)");

    // 10.3 Margin Protection (Loss Prevention against Trade Price / Cost Floor)
    domain::Item tightMarginItem;
    tightMarginItem.id = 103;
    tightMarginItem.name = "Expensive Antibiotic";
    tightMarginItem.salePrice = core::Money::fromRupees(1000.0);
    tightMarginItem.purchaseCost = core::Money::fromRupees(920.0);
    tightMarginItem.tp = tightMarginItem.purchaseCost;
    tightMarginItem.categoryDiscountable = true;
    tightMarginItem.categoryDefaultDiscountPct = 10.0;
    tightMarginItem.categoryMaxDiscountPct = 15.0;

    // Cashier attempts 15% discount (which would result in Rs. 850, below TP Rs. 920!)
    auto marginComp = services::PriceCalculator::calculateRowTotals(tightMarginItem, 1, 15.0);
    TEST_ASSERT(marginComp.wasClampedDueToLoss, "Loss prevention triggered when requested price drops below TP");
    TEST_ASSERT(marginComp.unitSalePrice == core::Money::fromRupees(920.0), "Sale price clamped strictly to TP floor (Rs. 920)");
    TEST_ASSERT(marginComp.unitDiscount == core::Money::fromRupees(80.0), "Discount clamped to maximum allowable margin (Rs. 80 / 8%)");

    // 10.4 Reverse Calculation from Bargained Net Price
    // Customer bargains to pay Rs. 450 on Rs. 500 item
    pharmaItem.discountPctOverride = std::nullopt;
    auto revComp = services::PriceCalculator::calculateReverseFromNetPrice(pharmaItem, 1, core::Money::fromRupees(450.0));
    TEST_ASSERT(revComp.discountPct == 10.0, "Reverse calculation derived exactly 10% from Rs. 450 net price");
    TEST_ASSERT(revComp.unitDiscount == core::Money::fromRupees(50.0), "Reverse calculation gave Rs. 50 discount");
    TEST_ASSERT(revComp.unitSalePrice == core::Money::fromRupees(450.0), "Unit sale price matches entered bargained price");

    // Reverse calculation below TP is clamped
    auto revLossComp = services::PriceCalculator::calculateReverseFromNetPrice(tightMarginItem, 1, core::Money::fromRupees(800.0));
    TEST_ASSERT(revLossComp.wasClampedDueToLoss, "Reverse calculation clamped when bargained price is below TP");
    TEST_ASSERT(revLossComp.unitSalePrice == core::Money::fromRupees(920.0), "Reverse calculation clamped to TP floor (Rs. 920)");

    // 10.5 Global Bill Discount (F4) protecting FMCG lines
    std::vector<domain::CartItem> testCart;
    domain::CartItem cPharma;
    cPharma.itemId = 102;
    cPharma.itemName = "Panadol 500mg Tablets";
    cPharma.displayQty = 2;
    cPharma.unitMrp = core::Money::fromRupees(500.0);
    cPharma.unitTp = core::Money::fromRupees(400.0);
    cPharma.isDiscountable = true;
    cPharma.totalGross = core::Money::fromRupees(1000.0);
    testCart.push_back(cPharma);

    domain::CartItem cFmcg;
    cFmcg.itemId = 101;
    cFmcg.itemName = "Pampers Baby Dry Diapers";
    cFmcg.displayQty = 1;
    cFmcg.unitMrp = core::Money::fromRupees(2500.0);
    cFmcg.unitTp = core::Money::fromRupees(2300.0);
    cFmcg.isDiscountable = false;
    cFmcg.totalGross = core::Money::fromRupees(2500.0);
    testCart.push_back(cFmcg);

    bool anySkipped = false;
    bool anyClamped = false;
    services::PriceCalculator::applyGlobalBillDiscount(testCart, 10.0, anySkipped, anyClamped);

    TEST_ASSERT(anySkipped, "Global bill discount reported FMCG line was skipped");
    TEST_ASSERT(testCart[1].discountPct == 0.0, "FMCG line retained 0% discount under global bill discount");
    TEST_ASSERT(testCart[1].totalAmount == core::Money::fromRupees(2500.0), "FMCG line total retained full Rs. 2,500");
    TEST_ASSERT(testCart[0].discountPct == 10.0, "Pharma line received 10% global bill discount");
    TEST_ASSERT(testCart[0].totalDiscount == core::Money::fromRupees(100.0), "Pharma line total discount is Rs. 100");
    TEST_ASSERT(testCart[0].totalAmount == core::Money::fromRupees(900.0), "Pharma line net total is Rs. 900");

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
