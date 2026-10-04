#pragma once
#include <QString>
#include <QDateTime>
#include <QDate>
#include <vector>
#include <optional>
#include "core/Money.h"

namespace domain {

enum class UserRole {
    Admin,
    Manager,
    Cashier
};

struct User {
    int id{0};
    QString username;
    QString fullName;
    UserRole role{UserRole::Cashier};
    bool isActive{true};
};

struct Category {
    int id{0};
    QString name;
    QString description;
    
    // Departmental Discount Defaults
    bool isDiscountable{true};
    double defaultDiscountPct{0.0};
    double maxDiscountPct{15.0};
};

enum class StockUnitType {
    Piece,   // Standard piece / bottle / box
    Tablet,  // Loose tablet
    Strip,   // Strip of tablets
    Box      // Box containing strips/tablets
};

struct Item {
    int id{0};
    QString code;
    QString name;
    int categoryId{0};
    QString categoryName;
    QString brand;
    QString barcode;
    
    // Pricing: MRP (Printed Retail Price) & TP (Trade Price / Wholesale Cost)
    core::Money salePrice;          // MRP
    core::Money purchaseCost;       // TP
    core::Money tp;                 // Trade price floor (synced with purchaseCost)
    
    // Inventory settings
    int minStockAlert{10};
    bool isActive{true};
    
    // Pharmacy specifics
    bool isMedicine{false};
    QString genericName;
    QString strength;
    QString dosageForm;
    bool isPrescriptionRequired{false};
    
    // Multi-unit packaging conversions (lowest atomic unit is Tablet or Piece)
    // Example: 1 Box = 20 Strips, 1 Strip = 10 Tablets -> stripConversion = 10, boxConversion = 200
    int piecesPerStrip{1};
    int stripsPerBox{1};
    core::Money stripSalePrice;
    core::Money boxSalePrice;

    // Pricing & Margin Protection overrides (std::nullopt means inherit from Category)
    std::optional<bool> isDiscountableOverride{std::nullopt};
    std::optional<double> discountPctOverride{std::nullopt};
    double minMarginPct{0.0}; // Safety buffer above TP (Loss-prevention)

    // Resolved category defaults
    bool categoryDiscountable{true};
    double categoryDefaultDiscountPct{0.0};
    double categoryMaxDiscountPct{15.0};

    bool isDiscountable() const {
        return isDiscountableOverride.value_or(categoryDiscountable);
    }
    bool isDiscountable(bool fallbackCatDiscountable) const {
        return isDiscountableOverride.value_or(fallbackCatDiscountable);
    }

    double defaultDiscountPercent() const {
        if (!isDiscountable()) return 0.0;
        return discountPctOverride.value_or(categoryDefaultDiscountPct);
    }
    double defaultDiscountPercent(bool fallbackCatDiscountable, double fallbackCatDefaultPct) const {
        if (!isDiscountable(fallbackCatDiscountable)) return 0.0;
        return discountPctOverride.value_or(fallbackCatDefaultPct);
    }
};

struct Batch {
    int id{0};
    int itemId{0};
    QString batchNumber;
    QDate expiryDate;
    core::Money costPrice;
    core::Money salePrice;
    int quantityRemaining{0}; // stored in lowest atomic unit (tablets / pieces)
};

enum class StockStatus {
    Available,
    LowStock,
    OutOfStock,
    NearExpiry,
    Expired
};

struct StockItemView {
    int itemId{0};
    QString code;
    QString itemName;
    QString categoryName;
    int totalAtomicQty{0}; // e.g., total tablets or pieces
    int minStock{0};
    StockStatus status{StockStatus::Available};
    std::optional<QDate> nearestExpiry;
    QString nearestBatch;
    core::Money retailPrice;
};

enum class PaymentType {
    Cash,
    Udhaar,     // Credit
    BankTransfer,
    Easypaisa,
    JazzCash,
    Card
};

struct PaymentAllocation {
    PaymentType type{PaymentType::Cash};
    core::Money amount;
    QString reference; // e.g. JazzCash TID, EasyPaisa mobile, Card last 4
};

struct Customer {
    int id{0};
    QString name;
    QString phone;
    QString address;
    core::Money baqaya; // Outstanding balance
    QString notes;
};

struct Supplier {
    int id{0};
    QString name;
    QString phone;
    QString address;
    core::Money baqaya; // Amount owed to supplier
    QString notes;
};

enum class SaleUnitSelection {
    PieceOrTablet,
    Strip,
    Box
};

struct CartItem {
    int itemId{0};
    QString itemName;
    QString barcode;
    SaleUnitSelection unitSelection{SaleUnitSelection::PieceOrTablet};
    int displayQty{1};        // User entered quantity (e.g. 2 strips)
    int atomicUnitsPerQty{1}; // e.g. 10 tablets per strip
    int totalAtomicQty{1};    // displayQty * atomicUnitsPerQty
    
    // Complete Pricing Architecture
    core::Money unitMrp;          // Base MRP for selected unit
    core::Money unitTp;           // Trade Price (Cost floor)
    bool isDiscountable{true};    // Strict lock (FMCG = false)
    double discountPct{0.0};      // Active row discount % (e.g. 10.0)
    core::Money unitDiscount;     // unitMrp * (discountPct / 100)
    core::Money unitSalePrice;    // unitMrp - unitDiscount (Net per unit)
    core::Money unitPrice;        // Alias for compatibility (= unitSalePrice)
    
    core::Money totalGross;       // unitMrp * displayQty
    core::Money totalDiscount;    // unitDiscount * displayQty
    core::Money totalAmount;      // unitSalePrice * displayQty (Net Line Total)
    
    // FEFO Batch Allocation details
    int batchId{0};
    QString batchNumber;
    QDate expiryDate;
    bool isExpired{false};
};

enum class SaleStatus {
    Completed,
    Cancelled,
    Returned
};

struct Sale {
    int id{0};
    QString billNumber;
    int customerId{1}; // 1 = Walk-in Customer
    QString customerName{"Walk-in Customer"};
    PaymentType paymentType{PaymentType::Cash};
    core::Money subtotal;
    core::Money discount;
    core::Money netTotal;
    core::Money cashReceived;
    core::Money changeGiven;
    QDateTime createdAt;
    int userId{0};
    QString cashierName;
    SaleStatus status{SaleStatus::Completed};
    QString cancelReason;
    
    std::vector<CartItem> items;
    std::vector<PaymentAllocation> payments;
};

struct HeldBill {
    QString holdId;
    QString customerName;
    QDateTime holdTime;
    std::vector<CartItem> items;
    core::Money total;
};

struct PurchaseItem {
    int itemId{0};
    QString itemName;
    int atomicQty{0};
    core::Money unitCost;
    core::Money totalCost;
    QString batchNumber;
    QDate expiryDate;
};

struct Purchase {
    int id{0};
    QString invoiceNumber;
    int supplierId{0};
    QString supplierName;
    bool isCashMarketPurchase{false}; // Local market / Shah Alam cash purchase
    core::Money totalCost;
    core::Money amountPaid;
    QDateTime createdAt;
    int userId{0};
    QString notes;
    
    std::vector<PurchaseItem> items;
};

enum class MovementType {
    Purchase,
    Sale,
    SaleReturn,
    PurchaseReturn,
    OpeningStock,
    StockCorrectionAdd,
    StockCorrectionSub,
    DamagedStock,
    ExpiredStock
};

struct StockMovement {
    int id{0};
    int itemId{0};
    int batchId{0};
    MovementType type{MovementType::Sale};
    int quantityDelta{0};
    int balanceAfter{0};
    QString reason;
    int referenceId{0};
    QDateTime timestamp;
    int userId{0};
};

struct CashSession {
    int id{0};
    int counterId{1};
    int userId{0};
    QString cashierName;
    QDateTime openedAt;
    std::optional<QDateTime> closedAt;
    core::Money openingFloat;
    core::Money cashSales;
    core::Money cashReceivedCustomer;
    core::Money cashReturns;
    core::Money expectedCash;
    core::Money actualCash;
    core::Money difference;
    QString discrepancyNotes;
    bool isOpen{true};
};

} // namespace domain
