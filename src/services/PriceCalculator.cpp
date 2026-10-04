#include "services/PriceCalculator.h"
#include <algorithm>
#include <cmath>

namespace services {

PricingComputation PriceCalculator::calculateRowTotals(
    const domain::Item& item,
    int qty,
    double requestedDiscountPct,
    core::Money customUnitMrp,
    core::Money customUnitTp)
{
    return calculateRowTotals(
        item,
        item.categoryDiscountable,
        item.categoryDefaultDiscountPct,
        item.categoryMaxDiscountPct,
        qty,
        requestedDiscountPct,
        customUnitMrp,
        customUnitTp
    );
}

PricingComputation PriceCalculator::calculateRowTotals(
    const domain::Item& item,
    bool categoryDiscountable,
    double categoryDefaultDiscPct,
    double categoryMaxDiscPct,
    int qty,
    double requestedDiscountPct,
    core::Money customUnitMrp,
    core::Money customUnitTp)
{
    PricingComputation res;
    core::Money baseMrp = customUnitMrp.isPositive() ? customUnitMrp : item.salePrice;
    core::Money baseTp = customUnitTp.isPositive() ? customUnitTp : (item.tp.isPositive() ? item.tp : item.purchaseCost);

    res.unitMrp = baseMrp;
    res.unitTp = baseTp;
    res.totalGross = baseMrp * qty;

    // Edge Case 1: The "Non-Discountable" Lock (FMCG / Milk / Cosmetics / Diapers)
    bool discountable = item.isDiscountable(categoryDiscountable);
    res.isDiscountable = discountable;

    if (!discountable) {
        res.discountPct = 0.0;
        res.unitDiscount = core::Money(0);
        res.unitSalePrice = baseMrp;
        res.totalDiscount = core::Money(0);
        res.totalAmount = res.totalGross;
        res.wasClampedDueToLoss = false;
        return res;
    }

    // Edge Case 2: Category-Level vs Item-Level Defaults
    double targetPct = requestedDiscountPct;
    if (targetPct < 0.0) {
        targetPct = item.defaultDiscountPercent(categoryDiscountable, categoryDefaultDiscPct);
    }

    // Clamp discount percentage to category ceiling
    double maxCeiling = (categoryMaxDiscPct > 0.0) ? categoryMaxDiscPct : 15.0;
    double effectivePct = std::clamp(targetPct, 0.0, maxCeiling);

    int64_t discountPaisa = static_cast<int64_t>(std::round(baseMrp.paisa() * (effectivePct / 100.0)));
    core::Money trialDiscount(discountPaisa);
    core::Money trialSalePrice = baseMrp - trialDiscount;

    // Edge Case 3: Margin Protection (Loss Prevention against Trade Price)
    core::Money floorPrice = baseTp * (1.0 + (item.minMarginPct / 100.0));
    if (trialSalePrice < floorPrice && baseMrp >= floorPrice) {
        trialSalePrice = floorPrice;
        trialDiscount = baseMrp - floorPrice;
        if (baseMrp.paisa() > 0) {
            effectivePct = (static_cast<double>(trialDiscount.paisa()) * 100.0) / baseMrp.paisa();
        }
        res.wasClampedDueToLoss = true;
    }

    res.discountPct = effectivePct;
    res.unitDiscount = trialDiscount;
    res.unitSalePrice = trialSalePrice;
    res.totalDiscount = trialDiscount * qty;
    res.totalAmount = trialSalePrice * qty;

    return res;
}

PricingComputation PriceCalculator::calculateReverseFromNetPrice(
    const domain::Item& item,
    int qty,
    core::Money requestedUnitSalePrice,
    core::Money customUnitMrp,
    core::Money customUnitTp)
{
    return calculateReverseFromNetPrice(
        item,
        item.categoryDiscountable,
        item.categoryMaxDiscountPct,
        qty,
        requestedUnitSalePrice,
        customUnitMrp,
        customUnitTp
    );
}

PricingComputation PriceCalculator::calculateReverseFromNetPrice(
    const domain::Item& item,
    bool categoryDiscountable,
    double categoryMaxDiscPct,
    int qty,
    core::Money requestedUnitSalePrice,
    core::Money customUnitMrp,
    core::Money customUnitTp)
{
    PricingComputation res;
    core::Money baseMrp = customUnitMrp.isPositive() ? customUnitMrp : item.salePrice;
    core::Money baseTp = customUnitTp.isPositive() ? customUnitTp : (item.tp.isPositive() ? item.tp : item.purchaseCost);

    res.unitMrp = baseMrp;
    res.unitTp = baseTp;
    res.totalGross = baseMrp * qty;

    // Edge Case 1: Non-Discountable Lock (Cannot bargain down FMCG)
    bool discountable = item.isDiscountable(categoryDiscountable);
    res.isDiscountable = discountable;
    if (!discountable) {
        res.discountPct = 0.0;
        res.unitDiscount = core::Money(0);
        res.unitSalePrice = baseMrp;
        res.totalDiscount = core::Money(0);
        res.totalAmount = res.totalGross;
        res.wasClampedDueToLoss = false;
        return res;
    }

    core::Money finalSalePrice = requestedUnitSalePrice;

    // Cannot sell higher than printed MRP
    if (finalSalePrice > baseMrp) {
        finalSalePrice = baseMrp;
    }

    // Edge Case 3: Margin Protection (Loss Prevention against Trade Price)
    core::Money floorPrice = baseTp * (1.0 + (item.minMarginPct / 100.0));
    if (finalSalePrice < floorPrice && baseMrp >= floorPrice) {
        finalSalePrice = floorPrice;
        res.wasClampedDueToLoss = true;
    }

    core::Money unitDiscount = baseMrp - finalSalePrice;
    double derivedPct = 0.0;
    if (baseMrp.paisa() > 0) {
        derivedPct = (static_cast<double>(unitDiscount.paisa()) * 100.0) / baseMrp.paisa();
    }

    double maxCeiling = (categoryMaxDiscPct > 0.0) ? categoryMaxDiscPct : 15.0;
    if (derivedPct > maxCeiling) {
        derivedPct = maxCeiling;
        int64_t maxDiscPaisa = static_cast<int64_t>(std::round(baseMrp.paisa() * (derivedPct / 100.0)));
        unitDiscount = core::Money(maxDiscPaisa);
        finalSalePrice = baseMrp - unitDiscount;
    }

    res.discountPct = derivedPct;
    res.unitDiscount = unitDiscount;
    res.unitSalePrice = finalSalePrice;
    res.totalDiscount = unitDiscount * qty;
    res.totalAmount = finalSalePrice * qty;

    return res;
}

void PriceCalculator::applyGlobalBillDiscount(
    std::vector<domain::CartItem>& cart,
    double globalBillDiscountPct,
    bool& anyFmcgSkipped,
    bool& anyClampedDueToLoss)
{
    anyFmcgSkipped = false;
    anyClampedDueToLoss = false;

    for (auto& row : cart) {
        // Edge Case 1: The "Non-Discountable" Lock (Strictly protect FMCG)
        if (!row.isDiscountable) {
            row.discountPct = 0.0;
            row.unitDiscount = core::Money(0);
            row.unitSalePrice = row.unitMrp;
            row.unitPrice = row.unitMrp;
            row.totalDiscount = core::Money(0);
            row.totalAmount = row.totalGross;
            anyFmcgSkipped = true;
            continue;
        }

        // Forward calculate for eligible pharma line
        double effectivePct = std::clamp(globalBillDiscountPct, 0.0, 100.0);
        int64_t discPaisa = static_cast<int64_t>(std::round(row.unitMrp.paisa() * (effectivePct / 100.0)));
        core::Money trialDiscount(discPaisa);
        core::Money trialSalePrice = row.unitMrp - trialDiscount;

        // Margin Protection check against Trade Price floor
        if (trialSalePrice < row.unitTp && row.unitMrp >= row.unitTp) {
            trialSalePrice = row.unitTp;
            trialDiscount = row.unitMrp - row.unitTp;
            if (row.unitMrp.paisa() > 0) {
                effectivePct = (static_cast<double>(trialDiscount.paisa()) * 100.0) / row.unitMrp.paisa();
            }
            anyClampedDueToLoss = true;
        }

        row.discountPct = effectivePct;
        row.unitDiscount = trialDiscount;
        row.unitSalePrice = trialSalePrice;
        row.unitPrice = trialSalePrice;
        row.totalDiscount = trialDiscount * row.displayQty;
        row.totalAmount = trialSalePrice * row.displayQty;
    }
}

} // namespace services
