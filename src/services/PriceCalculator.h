#pragma once
#include "domain/Models.h"
#include <vector>

namespace services {

struct PricingComputation {
    core::Money unitMrp;
    core::Money unitTp;
    core::Money unitDiscount;
    core::Money unitSalePrice;
    double discountPct{0.0};
    core::Money totalGross;
    core::Money totalDiscount;
    core::Money totalAmount; // Net Total
    bool isDiscountable{true};
    bool wasClampedDueToLoss{false};
};

class PriceCalculator {
public:
    // Forward calculation: User enters or inherits Discount %
    static PricingComputation calculateRowTotals(
        const domain::Item& item,
        int qty,
        double requestedDiscountPct = -1.0,
        core::Money customUnitMrp = core::Money(0),
        core::Money customUnitTp = core::Money(0)
    );

    static PricingComputation calculateRowTotals(
        const domain::Item& item,
        bool categoryDiscountable,
        double categoryDefaultDiscPct,
        double categoryMaxDiscPct,
        int qty,
        double requestedDiscountPct,
        core::Money customUnitMrp = core::Money(0),
        core::Money customUnitTp = core::Money(0)
    );

    // Reverse calculation: Cashier enters bargained Net Sale Price (e.g. 450 on 500 item)
    static PricingComputation calculateReverseFromNetPrice(
        const domain::Item& item,
        int qty,
        core::Money requestedUnitSalePrice,
        core::Money customUnitMrp = core::Money(0),
        core::Money customUnitTp = core::Money(0)
    );

    static PricingComputation calculateReverseFromNetPrice(
        const domain::Item& item,
        bool categoryDiscountable,
        double categoryMaxDiscPct,
        int qty,
        core::Money requestedUnitSalePrice,
        core::Money customUnitMrp = core::Money(0),
        core::Money customUnitTp = core::Money(0)
    );

    // Global Bill Discount: Fair, loss-protected distribution that strictly protects FMCG lines
    static void applyGlobalBillDiscount(
        std::vector<domain::CartItem>& cart,
        double globalBillDiscountPct,
        bool& anyFmcgSkipped,
        bool& anyClampedDueToLoss
    );
};

} // namespace services
