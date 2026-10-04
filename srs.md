# SRS — Pakistani Local Pharmacy & General Retail POS

**Product Type:** Windows Desktop POS + Inventory + Pharmacy/Retail Management
**Target Market:** Pakistani local pharmacies, medical stores, mini-marts attached to pharmacies, and general health/beauty retail stores
**Store Model:** Single physical store, multiple computers/cash counters
**Primary Users:** Cashier, Pharmacist/Store Operator, Manager, Owner/Admin
**Priority:** Production-ready, simple, fast, non-technical, keyboard-friendly

---

## 1. Recommended Technology Stack

### Desktop Application

**C++20/23 + Qt 6 Widgets**

Qt is the strongest fit for this particular application because the product needs a native-feeling Windows desktop interface, reusable desktop components, printing, keyboard-heavy workflows, dialogs, tables, barcode scanners, and low overhead without requiring Electron.

Qt can be used through its Community/open-source licensing subject to LGPL/GPL obligations, or commercially if the application is proprietary and those obligations are not desirable. ([Qt Documentation][1])

### Recommended stack

| Area            | Technology                                |
| --------------- | ----------------------------------------- |
| Language        | C++20                                     |
| UI              | Qt 6 Widgets                              |
| UI Styling      | Qt Style Sheets + reusable custom widgets |
| Database        | PostgreSQL recommended                    |
| Local fallback  | SQLite for single-PC/offline scenarios    |
| ORM/Data Layer  | Thin repository/data-access layer         |
| Reports         | Qt printing/PDF                           |
| Barcode         | USB HID barcode scanner                   |
| Receipt Printer | ESC/POS thermal printer                   |
| Cash Drawer     | Receipt-printer driven                    |
| OS              | Windows 10/11                             |
| Build           | CMake + MinGW/MSVC Build Tools            |
| IDE             | Qt Creator                                |
| Installer       | Inno Setup                                |
| Configuration   | `.ini`/JSON where appropriate             |
| Backup          | Automatic scheduled database backups      |

### Important architecture decision: multiple computers

Because your requirement is **multiple computers accessing the same store data**, I would **not** make a shared SQLite file on a network folder the production architecture.

SQLite supports multiple readers but only one writer at a time, and SQLite explicitly warns against relying on network filesystems for simultaneous access. ([SQLite][2])

For a real pharmacy with several computers:

> **Central PostgreSQL database + C++/Qt client on every computer**

Architecture:

```text
                    ┌──────────────────┐
                    │   Store Network  │
                    └────────┬─────────┘
                             │
              ┌──────────────┴──────────────┐
              │                             │
      ┌───────▼────────┐          ┌─────────▼───────┐
      │ POS Computer 1 │          │ POS Computer 2  │
      │ C++ + Qt       │          │ C++ + Qt        │
      └───────┬────────┘          └─────────┬───────┘
              │                             │
              └──────────────┬──────────────┘
                             │
                    ┌────────▼────────┐
                    │   PostgreSQL    │
                    │  Store Database │
                    └─────────────────┘
```

This provides one authoritative stock, customer, supplier, sale and ledger system for the whole store.

---

# 2. Product Vision

The software should behave like a **professional Pakistani pharmacy counter**, not like accounting software.

The cashier should be able to perform:

> **Scan → Add → Receive Cash → Print**

with almost no mouse usage.

A new employee should be able to understand the POS after a few minutes without knowing terms such as:

* Database
* SKU
* Entity
* Transaction ID
* Ledger ID
* Inventory Adjustment
* Stock Movement
* API
* Batch Entity
* Credit Sale Transaction

These are developer concepts and **must never appear in normal user-facing screens.**

---

# 3. Core Design Principles

## 3.1 Non-technical language

Use:

| Avoid                | Use              |
| -------------------- | ---------------- |
| Product              | Item             |
| SKU                  | Item Code        |
| Inventory Adjustment | Stock Correction |
| Transaction          | Sale / Purchase  |
| Customer Account     | Customer         |
| Supplier Account     | Supplier         |
| Debit/Credit         | Jama / Udhaar    |
| Outstanding Balance  | Baqaya           |
| Stock Movement       | Stock History    |
| Entity               | —                |
| Batch Entity         | Batch            |
| Created At           | Date             |
| Updated At           | Last Updated     |
| `CREDIT_SALE`        | Udhaar Sale      |
| `OPENING_BAL`        | Opening Stock    |
| `ADJUSTMENT`         | Stock Correction |

Even better: **hide technical terminology completely wherever possible.**

---

# 4. Main Modules

The application shall contain:

1. **Sale**
2. **Items**
3. **Stock**
4. **Purchases**
5. **Expiry**
6. **Suppliers**
7. **Customers**
8. **Khata**
9. **Returns**
10. **Reports**
11. **Users & Security**
12. **Settings**
13. **Backup & Restore**

The POS should remain the primary screen.

---

# 5. Main Navigation

Recommended navigation:

```text
┌──────────────────────────────────────────────────────────────┐
│ A1 Pharmacy                         User: Ahmed       11:42 AM│
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  Sale     Items     Stock     Purchases     Customers        │
│  Suppliers     Khata     Returns     Reports     Settings    │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

No unnecessary dashboard graphics.

No:

* fake charts
* decorative cards
* gradients
* glass effects
* excessive icons
* huge empty spaces
* technical status panels

This is a **working counter application**, not a marketing dashboard.

---

# 6. POS / Sale Screen

This is the most important screen.

### Layout

```text
┌─────────────────────────────────────────────────────────────┐
│ SALE                                      Bill # 001245      │
├─────────────────────────────────────────────────────────────┤
│ Search / Scan Item                                           │
│ [________________________________________________________]  │
├──────┬────────────────────────┬──────┬────────┬────────────┤
│      │ Item                   │ Qty  │ Price  │ Total      │
├──────┼────────────────────────┼──────┼────────┼────────────┤
│      │ Panadol 500mg          │  2   │ 50     │ 100        │
│      │ Surf Excel             │  1   │ 280    │ 280        │
│      │ Baby Pampers Medium    │  1   │ 450    │ 450        │
├──────┴────────────────────────┴──────┴────────┴────────────┤
│                                                             │
│ Items: 4                              TOTAL                 │
│                                                             │
│                                      Rs. 830                │
│                                                             │
│ [F2 Quantity] [F4 Discount] [F8 Hold] [F9 Pay & Print]     │
└─────────────────────────────────────────────────────────────┘
```

---

# 7. Barcode Workflow

USB barcode scanners should work as keyboard input.

Normal workflow:

```text
Scan
 ↓
Item found
 ↓
Item added
 ↓
Cursor returns to search field
 ↓
Next scan
```

No dialog should appear for every successful scan.

If item does not exist:

```text
Item not found.

[Add New Item] [Search Again] [Cancel]
```

The cashier must not be forced into the Items module.

---

# 8. Quantity Handling

Examples:

### Normal

Scan:

```text
Panadol
```

Result:

```text
Panadol × 1
```

### Multiple quantity

Cashier can:

```text
F2 → 5 → Enter
```

or:

```text
5 × Scan
```

depending on configured workflow.

### Loose tablets

The system must support:

```text
Box
Strip
Tablet
```

where applicable.

Example:

```text
Panadol 500mg
Box = 20 strips
Strip = 10 tablets
```

If the store sells individual tablets:

```text
Customer buys 3 tablets
```

stock decreases by 3 tablets.

The system must maintain correct conversion between purchase/sale units.

---

# 9. Product Categories

The software must not assume that everything is medicine.

Examples:

### Medicines

* Tablets
* Capsules
* Syrups
* Drops
* Injections
* Creams
* Ointments

### Baby

* Pampers
* Baby wipes
* Baby powder
* Feeding bottles

### Personal Care

* Face wash
* Soap
* Shampoo
* Toothpaste
* Toothbrush

### Beverages

* Cold drinks
* Juices
* Energy drinks
* Water

### General

* First aid
* Bandages
* Cotton
* Masks
* Thermometers
* Sanitizers

The category system must be configurable.

---

# 10. Item Management

Each item can contain:

### Basic information

* Item name
* Category
* Manufacturer/Brand
* Barcode
* Item code
* Sale price
* Purchase price
* Minimum stock
* Active/Inactive

### Pharmacy information

Where applicable:

* Generic name
* Strength
* Dosage form
* Pack size
* Prescription-required flag
* Batch tracking
* Expiry tracking

### Retail information

* Unit
* Selling unit
* Purchase unit
* Conversion
* Retail price
* Wholesale price if enabled

---

# 11. Item Creation Must Be Fast

The system should support:

```text
Items
→ Add Item
```

but also:

```text
POS
→ Item not found
→ Add Item
```

Minimal required fields:

```text
Item Name
Category
Sale Price
```

Optional details can be added later.

This prevents staff from being blocked by lengthy forms.

---

# 12. Stock Management

Stock must be authoritative.

Every stock-changing operation must be traceable.

Examples:

```text
Purchase
Sale
Sale Return
Purchase Return
Opening Stock
Stock Correction
Expired Stock
Damaged Stock
```

The system must never simply overwrite the quantity without recording why it changed.

---

# 13. Stock Screen

Recommended columns:

| Item          | Current Stock | Minimum | Status | Expiry   |
| ------------- | ------------: | ------: | ------ | -------- |
| Panadol 500mg |           120 |      20 | OK     | —        |
| Brufen        |             8 |      20 | Low    | —        |
| Syrup X       |             3 |      10 | Low    | Dec 2026 |

Status:

* Available
* Low Stock
* Out of Stock
* Near Expiry
* Expired

Use clear language rather than technical indicators.

---

# 14. Batch & Expiry

For medicines and expiry-sensitive products:

```text
Item
 ├── Batch A
 │    ├── Qty
 │    ├── Purchase Date
 │    └── Expiry
 │
 └── Batch B
      ├── Qty
      ├── Purchase Date
      └── Expiry
```

The system should prefer the batch that expires first.

### Example

```text
Panadol

Batch A → Expiry Jan 2027 → 30
Batch B → Expiry Aug 2027 → 100
```

Sale should consume:

```text
Batch A first
```

This is FEFO:

> First Expiry, First Out.

The cashier should **not need to understand FEFO**.

---

# 15. Expiry Management

Dedicated screen:

```text
Expiry

Expired
Near Expiry
All Expiry
```

Filters:

```text
Next 30 Days
Next 60 Days
Next 90 Days
Expired
```

Example:

```text
Panadol Syrup
Batch: ABC123
Expiry: 15-Nov-2026
Qty: 12
```

Actions:

```text
Mark for Return
Remove from Sale
Stock Correction
Print List
```

---

# 16. Purchase Management

Purchase workflow:

```text
Supplier
 ↓
Purchase
 ↓
Items
 ↓
Batch
 ↓
Expiry
 ↓
Quantity
 ↓
Cost
 ↓
Save
 ↓
Stock Updated
```

Purchase screen:

```text
Supplier: ABC Medical Store

Item                 Qty   Cost    Batch      Expiry
-----------------------------------------------------
Panadol              100   42      P123       08/2028
Brufen                50   55      B221       04/2027
```

---

# 17. Pakistani Cash Purchase Scenario

A pharmacy may buy goods from a market wholesaler without a formal invoice.

Example:

> Owner buys 50 Panadol boxes from Shah Alam market and pays cash.

The software must allow:

```text
Supplier:
Cash Purchase / Local Market
```

without forcing:

* invoice number
* supplier account
* formal purchase order

Required:

```text
Items
Quantity
Cost
Batch
Expiry where applicable
```

Optional:

```text
Supplier
Invoice Number
Notes
```

This reflects actual local retail practice.

---

# 18. Supplier Management

Supplier profile:

```text
Supplier Name
Phone
Address
Notes
Opening Balance
```

Supplier account should support:

```text
Purchase
Payment
Purchase Return
Balance
```

Example:

```text
ABC Pharma

Purchase       Rs. 50,000
Payment        Rs. 30,000
Baqaya         Rs. 20,000
```

---

# 19. Customer Management

Customer information should remain minimal.

```text
Name
Phone
Address
Notes
```

Phone number should not be mandatory for every customer.

This is important because most local pharmacy customers are walk-in customers.

Default:

```text
Walk-in Customer
```

No form should appear for every cash sale.

---

# 20. Credit / Udhaar Sales

A cashier should be able to convert a sale into credit.

Example:

```text
Total: Rs. 2,500

[Cash]
[Udhaar]
```

Selecting Udhaar:

```text
Select Customer
```

Then:

```text
Sale completed
Customer balance + Rs. 2,500
```

No unnecessary accounting terminology.

---

# 21. Khata

Customer Khata should show:

```text
Customer: Ali Khan

Previous Baqaya       Rs. 4,000
New Sale              Rs. 1,200
Payment               Rs. 2,000
-----------------------------
Remaining Baqaya     Rs. 3,200
```

Buttons:

```text
Receive Payment
New Sale
View History
Print Khata
```

---

# 22. Payment Collection

Fast workflow:

```text
Customers
→ Select Customer
→ Receive Payment
→ Amount
→ Save
```

Example:

```text
Baqaya: Rs. 8,500

Received:
[5,000]

Remaining:
Rs. 3,500
```

Receipt can optionally be printed.

---

# 23. Sales Returns

Return workflow:

```text
Returns
→ Sale Return
→ Find Bill
→ Select Items
→ Quantity
→ Return
```

Example:

```text
Bill #1254

Panadol       2
Brufen        1
Syrup         1
```

Customer returns:

```text
Brufen × 1
```

System should:

* reduce sale
* increase stock where appropriate
* adjust customer balance/payment
* preserve original bill history
* record return reason

---

# 24. Purchase Returns

Example:

Supplier sends:

```text
Syrup
Batch expired/near expiry
```

User:

```text
Purchases
→ Purchase Return
→ Select Supplier
→ Select Item
→ Quantity
→ Return
```

Stock decreases and supplier balance is adjusted.

---

# 25. Stock Correction

Only authorized users should correct stock.

Example:

System says:

```text
Panadol = 100
```

Physical count:

```text
Panadol = 97
```

Manager can enter:

```text
Actual Quantity: 97

Reason:
Damaged / Missing / Counting Error
```

The software records:

```text
Before: 100
After: 97
Difference: -3
Reason: Counting Error
User: Manager
Date: ...
```

Never silently overwrite stock.

---

# 26. First Aid / General Retail Workflow

The system must work equally well for:

```text
Bandage
Cotton
Dettol
Soap
Face Wash
Pampers
Cold Drink
```

No pharmacy-specific information should become mandatory for ordinary items.

For example:

```text
Lux Soap
```

should not require:

* batch
* generic name
* strength
* dosage form

unless the store chooses to track those details.

---

# 27. Dashboard

Keep it simple.

The home screen should show only useful information:

```text
Today's Sales       Rs. 85,400
Today's Bills       126
Credit Sales        Rs. 12,500
Low Stock           18
Near Expiry         7
```

Optional quick actions:

```text
New Sale
Receive Payment
Add Item
New Purchase
View Expiry
```

No decorative graphs unless genuinely useful.

---

# 28. Reports

## Sales

* Daily Sales
* Date-wise Sales
* Item-wise Sales
* Cash Sales
* Credit Sales
* Returns
* Discounts

## Stock

* Current Stock
* Low Stock
* Out of Stock
* Stock Value
* Expiry
* Stock History

## Customers

* Customer Balances
* Customer Sales
* Payments
* Khata History

## Suppliers

* Supplier Balances
* Purchases
* Payments
* Purchase Returns

## Profit

* Daily Profit
* Date Range Profit
* Item Profit

Profit calculations must use actual purchase cost associated with sold stock, not merely current item cost.

---

# 29. User Roles

### Owner/Admin

Full access:

* Sales
* Purchases
* Stock
* Returns
* Customers
* Suppliers
* Reports
* Users
* Settings
* Backup
* Stock correction

### Manager

Operational access:

* Sales
* Purchases
* Stock
* Returns
* Customers
* Suppliers
* Reports

Restricted:

* User management
* Database restore
* critical settings

### Cashier

Primarily:

* Sale
* Customer selection
* Payment collection
* Sale return if authorized

Cannot:

* change purchase cost
* modify stock directly
* delete bills
* alter historical records
* access sensitive reports

---

# 30. Security

Every important action should record:

```text
User
Date
Time
Action
Affected item
Old value
New value
Reason
```

Critical actions require permission.

Examples:

```text
Delete Sale
Change Sale Price
Change Purchase Cost
Stock Correction
Delete Customer
Delete Supplier
Return Without Original Bill
```

Prefer **void/cancel** over permanent deletion.

---

# 31. Sale Cancellation

A completed sale should not simply disappear.

Instead:

```text
Cancel Bill
```

requires authorization and reason.

Example:

```text
Bill #1205

Cancel reason:
Wrong customer / Duplicate bill

Authorized by:
Manager
```

The original record remains available in history.

---

# 32. Pricing

Each item should support:

```text
Purchase Cost
Sale Price
```

Optional:

```text
Discount
Maximum Retail Price
```

The application must prevent accidental sale below configured limits where the store chooses to enforce them.

---

# 33. Cash Payment

Payment dialog:

```text
TOTAL

Rs. 1,275

Cash Received
[ 2,000 ]

Change
Rs. 725

[Complete Sale]
```

The change should be calculated automatically.

---

# 34. Multiple Payment Types

Initial version:

* Cash
* Credit / Udhaar

Recommended optional:

* Bank Transfer
* Easypaisa
* JazzCash
* Card

The UI should not become complicated when these are enabled.

---

# 35. Receipt

80mm thermal receipt should contain:

```text
          ABC PHARMACY
       Phone / Address

Bill: 001245
Date: 03-Oct-2026

Panadol 500mg       2 x 50      100
Brufen              1 x 120     120
Face Wash           1 x 450     450
-----------------------------------
TOTAL                           670
Cash                          1,000
Change                          330

      Thank You
```

For customer privacy, configurable receipt fields should be supported.

---

# 36. Keyboard-First POS

Recommended shortcuts:

| Shortcut | Action       |
| -------- | ------------ |
| F1       | Help         |
| F2       | Quantity     |
| F3       | Item Search  |
| F4       | Discount     |
| F5       | Customer     |
| F6       | Payment      |
| F7       | Hold Bill    |
| F8       | Held Bills   |
| F9       | Pay & Print  |
| Esc      | Cancel/Close |
| Ctrl+K   | Quick Search |

Shortcuts must be displayed subtly where useful.

Example:

```text
Pay & Print  F9
```

Not:

```text
executeCheckoutTransaction()
```

---

# 37. Hold Bill

Cashier may need to temporarily pause a sale.

Example:

Customer A is shopping.

Customer B arrives with urgent medicine.

Cashier:

```text
F7 → Hold
```

Then starts Customer B's bill.

Later:

```text
F8 → Held Bills
→ Customer A
→ Continue
```

---

# 38. Customer Search

Search by:

```text
Name
Phone
```

Results should be fast.

Example:

```text
Ali
-----------------------------
Ali Khan        0300-xxxxxxx
Ali Medical     0345-xxxxxxx
```

---

# 39. Item Search

Search should support:

```text
Barcode
Item Name
Brand
Generic Name
Item Code
```

Example:

```text
pana
```

Results:

```text
Panadol 500mg
Panadol Extra
Panadol Syrup
```

Keyboard navigation:

```text
↑ ↓
Enter
```

---

# 40. UI/UX Design System

## Visual direction

Professional Pakistani retail desktop software.

Recommended:

* White/light neutral background
* Dark text
* Deep emerald primary color
* Slate/gray secondary colors
* Clear warning colors
* Strong contrast
* 36–40px normal controls
* 44px+ important POS controls
* Dense tables
* Minimal borders
* No excessive rounded cards

### Avoid

* Glassmorphism
* Large gradients
* Neon colors
* Huge cards
* Excessive animations
* Floating decorative widgets
* Fake analytics
* Excessive icons
* Tiny buttons
* Technical labels

---

# 41. Reusable UI Components

Create a consistent component system.

### Core components

```text
AppButton
AppInput
SearchBox
MoneyInput
QuantityInput
DateInput
ItemSearch
CustomerSearch
SupplierSearch
DataTable
PageHeader
Dialog
ConfirmDialog
PaymentDialog
EmptyState
StatusBadge
Toast
ReceiptPreview
```

All dialogs should share the same structure:

```text
┌─────────────────────────────────────┐
│ Title                         X      │
├─────────────────────────────────────┤
│                                     │
│ Content                             │
│                                     │
├─────────────────────────────────────┤
│                Cancel    Save       │
└─────────────────────────────────────┘
```

---

# 42. Error Messages

Never show technical errors.

### Bad

```text
SQLITE_CONSTRAINT_FOREIGNKEY
```

### Good

```text
This item cannot be removed because it has previous sales.
```

### Bad

```text
Connection timeout: PostgreSQL
```

### Good

```text
Store database is temporarily unavailable.
Please check the network connection.
```

---

# 43. Confirmation Dialogs

Do not ask unnecessary confirmations.

### Bad

```text
Are you absolutely sure you want to continue?
```

### Good

```text
Return 2 Panadol from Bill #1245?

[Return Items] [Cancel]
```

Confirmation must explain the actual consequence.

---

# 44. Database Model

Core entities:

```text
users
roles
permissions

items
categories
brands
units
item_barcodes

batches
stock_balances
stock_movements

customers
customer_ledger

suppliers
supplier_ledger

sales
sale_items
sale_payments

purchases
purchase_items
purchase_payments

sale_returns
sale_return_items

purchase_returns
purchase_return_items

cash_sessions
audit_logs
settings
```

---

# 45. Stock Architecture

Stock should be derived from authoritative stock-changing records.

Conceptually:

```text
Opening Stock
+ Purchases
+ Sale Returns
+ Stock Added
- Sales
- Purchase Returns
- Damaged
- Expired
- Stock Removed
=
Current Stock
```

Every mutation must be atomic.

A sale should never reach:

```text
Sale saved
but stock not reduced
```

or:

```text
Stock reduced
but sale not saved
```

Both must succeed together or both must fail.

---

# 46. Multi-Computer Data Consistency

Example:

Computer A:

```text
Panadol stock = 5
```

Computer B simultaneously sells:

```text
Panadol × 3
```

Computer A attempts:

```text
Panadol × 4
```

The system must prevent negative stock if negative stock is disabled.

Final result:

```text
Computer B → succeeds
Computer A → insufficient stock
```

not:

```text
Both sales succeed
Stock = -2
```

Database transactions and row-level protection should be used for stock-critical operations.

---

# 47. Network Failure

If the POS temporarily loses connection to the central database:

The application must clearly show:

```text
Store connection unavailable
```

It must **not pretend that a sale was successfully saved**.

Recommended production behavior:

```text
Online
→ normal operation

Connection lost
→ block database-dependent completion
→ preserve current bill locally
→ reconnect
→ retry
```

Offline sales can be introduced later as a carefully designed synchronization feature, but should not be improvised because stock synchronization becomes significantly more complex.

---

# 48. Backup

Automatic backup:

```text
Daily
Weekly
Manual
```

Recommended:

```text
Daily automatic backup
+
7–30 backup generations
+
manual backup button
```

Backup screen should say:

```text
Last Backup:
Today, 11:30 PM

[Backup Now]

Backup Location:
D:\Pharmacy Backups\
```

Not:

```text
Run pg_dump
```

---

# 49. Restore

Restore is an owner/admin operation.

Before restore:

```text
Restoring a backup will replace current store data.

Current backup:
03-Oct-2026

[Cancel]
[Continue]
```

The software should automatically create a safety backup before restoration.

---

# 50. Daily Closing

Recommended:

```text
Cash Counter
→ Close Day
```

Show:

```text
Opening Cash       Rs. 10,000
Cash Sales         Rs. 65,500
Cash Received      Rs. 15,000
Cash Returns       Rs.  2,000
Expected Cash      Rs. 88,500

Actual Cash        Rs. 88,300

Difference         -Rs. 200
```

Manager can close the day with a reason for discrepancy.

---

# 51. Cash Sessions

If multiple computers/cashiers are used, each cashier should optionally have a cash session.

Example:

```text
Ahmed
Counter 1
Opening Cash: Rs. 5,000
```

At closing:

```text
Expected: Rs. 37,500
Actual:   Rs. 37,500
Difference: Rs. 0
```

This provides real-world accountability.

---

# 52. Real-World Test Scenarios

These scenarios should be part of the acceptance test suite.

## Scenario 1 — Normal medicine sale

Stock:

```text
Panadol = 100
```

Customer buys:

```text
2
```

Expected:

```text
Sale = completed
Stock = 98
Receipt = printed
```

---

## Scenario 2 — Multiple computers

Computer A:

```text
Sale Panadol × 2
```

Computer B immediately checks stock.

Expected:

```text
Updated stock visible
```

---

## Scenario 3 — Last available item

Stock:

```text
Panadol = 1
```

Two computers attempt:

```text
Computer A → 1
Computer B → 1
```

Expected:

```text
One succeeds
One receives "Item is out of stock."
```

---

## Scenario 4 — Credit customer

Customer:

```text
Ali
```

Existing balance:

```text
Rs. 2,000
```

New sale:

```text
Rs. 750
```

Expected:

```text
New balance = Rs. 2,750
```

---

## Scenario 5 — Partial payment

Balance:

```text
Rs. 5,000
```

Customer pays:

```text
Rs. 2,000
```

Expected:

```text
Remaining = Rs. 3,000
```

---

## Scenario 6 — Sale return

Original:

```text
Panadol × 3
```

Customer returns:

```text
1
```

Expected:

```text
Sale adjusted
Stock +1
Customer/cash adjustment correct
Return recorded
```

---

## Scenario 7 — Expiry

Stock:

```text
Batch A → expiry in 15 days
Batch B → expiry in 8 months
```

Customer buys one.

Expected:

```text
Batch A sold first
```

---

## Scenario 8 — Expired product

Item:

```text
Syrup
Expiry = yesterday
```

Attempt sale.

Expected:

```text
Sale blocked
"Expired item cannot be sold."
```

Manager override should only exist if the business explicitly requires it, and any override must be logged.

---

## Scenario 9 — Mixed retail items

One bill:

```text
Panadol
Pampers
Face Wash
Soap
Cold Drink
Bandage
```

Expected:

```text
Single bill
Correct stock changes
Correct totals
Correct receipt
```

---

## Scenario 10 — Cash change

Total:

```text
Rs. 1,350
```

Customer gives:

```text
Rs. 2,000
```

Expected:

```text
Change = Rs. 650
```

---

## Scenario 11 — Wrong quantity

Cashier scans:

```text
Panadol
```

Then realizes customer wants:

```text
5
```

Expected:

```text
F2 → 5
```

No need to remove and rescan five times.

---

## Scenario 12 — Hold bill

Customer A bill:

```text
Rs. 1,500
```

Customer B arrives.

Expected:

```text
F7
Bill A held
New bill starts
```

After Customer B:

```text
F8
Bill A restored
```

---

## Scenario 13 — Unknown barcode

Scanner reads unknown barcode.

Expected:

```text
Item not found.

[Add Item]
[Search]
[Cancel]
```

No application crash.

---

## Scenario 14 — Cash purchase without supplier

Owner buys:

```text
50 soaps
```

from a local market seller.

No invoice.

Expected:

```text
Purchase can be recorded
Supplier can remain "Cash Purchase"
Stock increases
Cost is preserved
```

---

## Scenario 15 — Purchase with batch

Purchase:

```text
Syrup
Qty: 100
Batch: ABC12
Expiry: 2027
```

Expected:

```text
Stock +100
Batch created
Expiry appears in Expiry screen
```

---

## Scenario 16 — Damaged stock

Physical stock:

```text
50
```

Three damaged.

Expected:

```text
Stock = 47
Reason = Damaged
History records -3
User recorded
```

---

## Scenario 17 — Physical stock mismatch

System:

```text
100
```

Physical:

```text
97
```

Manager corrects.

Expected:

```text
Stock = 97
Difference = -3
Reason required
Audit history preserved
```

---

## Scenario 18 — Supplier balance

Purchase:

```text
Rs. 50,000
```

Payment:

```text
Rs. 30,000
```

Expected:

```text
Supplier baqaya = Rs. 20,000
```

---

## Scenario 19 — Customer search

Customer database:

```text
Muhammad Ali
Ali Khan
Ali Medical
```

Search:

```text
Ali
```

Expected:

All relevant matches appear immediately.

---

## Scenario 20 — Database failure during sale

Cashier is completing:

```text
Rs. 2,500 sale
```

Database connection fails.

Expected:

```text
Sale is NOT marked completed.
```

The user receives a clear message and the current bill remains available for retry.

---

# 53. Performance Requirements

Target workstation:

```text
Windows 10/11
8 GB RAM
SSD
```

Application requirements:

* Startup target: <2 seconds on normal SSD hardware
* Item search: near-instant
* Barcode response: <100 ms perceived response
* POS keyboard navigation: no visible lag
* Normal database operations: <500 ms target
* Reports should not freeze the POS UI
* Long reports run asynchronously

---

# 54. Reliability Requirements

The application must survive:

* sudden application close
* Windows restart
* power interruption
* database connection interruption
* printer unavailable
* barcode scanner disconnected
* invalid barcode
* duplicate barcode
* duplicate item
* insufficient stock
* expired product
* simultaneous sale from multiple computers

No operation should leave stock and financial records inconsistent.

---

# 55. Printer Failure

If receipt printer is disconnected:

```text
Sale Saved Successfully

Receipt could not be printed.

[Print Again]
[Close]
```

Do **not** roll back the sale merely because the printer failed.

This distinction is critical.

---

# 56. Barcode Requirements

Support:

* EAN-13
* EAN-8
* Code 128
* UPC where required
* custom store barcodes

Multiple barcodes may belong to one item.

Example:

```text
Panadol
Barcode 1
Barcode 2
Barcode 3
```

All should resolve to the same item where configured.

---

# 57. Item Duplicate Protection

The system should warn:

```text
An item with this barcode already exists.

Panadol 500mg

[Use Existing Item]
[Cancel]
```

Never silently create duplicate products.

---

# 58. Important UX Rule

**Do not force every pharmacy workflow into one generic form.**

For example:

A soap does not need:

```text
Generic Name
Strength
Dosage
Batch
Expiry
```

A medicine may need those.

Therefore fields should appear based on the item's configuration.

---

# 59. Technical Architecture

Recommended C++ structure:

```text
src/
├── app/
│   ├── Application
│   ├── AppContext
│   └── Configuration
│
├── core/
│   ├── Result
│   ├── Errors
│   ├── Money
│   ├── DateTime
│   └── Validation
│
├── database/
│   ├── Connection
│   ├── Transaction
│   ├── Migrations
│   └── Repositories
│
├── domain/
│   ├── Items
│   ├── Sales
│   ├── Purchases
│   ├── Stock
│   ├── Customers
│   ├── Suppliers
│   └── Accounting
│
├── services/
│   ├── SaleService
│   ├── StockService
│   ├── PurchaseService
│   ├── ReturnService
│   ├── LedgerService
│   └── BackupService
│
├── ui/
│   ├── components/
│   ├── sale/
│   ├── items/
│   ├── stock/
│   ├── purchases/
│   ├── customers/
│   ├── suppliers/
│   ├── reports/
│   └── settings/
│
└── printing/
    ├── ReceiptPrinter
    └── ReportPrinter
```

---

# 60. Separation of Responsibilities

UI should **not** directly modify database records.

Bad:

```text
SaleWindow → SQL UPDATE stock
```

Preferred:

```text
SaleWindow
    ↓
SaleService
    ↓
StockService
    ↓
Database Transaction
```

This makes stock behavior testable and prevents business rules from being duplicated throughout the UI.

---

# 61. Transaction Example

Completing a sale should conceptually execute:

```text
BEGIN

Create Sale
Create Sale Items
Calculate/record payment
Reduce Stock
Record Stock History
Update Customer Balance if credit
Record Audit

COMMIT
```

If any operation fails:

```text
ROLLBACK
```

The user sees one clear error.

---

# 62. No Permanent Destructive Deletes

Historical:

* sales
* returns
* purchases
* payments
* stock changes

should not normally be physically deleted.

Use:

```text
Cancel
Void
Reverse
Return
Correction
```

This is essential for pharmacy/retail accountability.

---

# 63. First Version Scope

### Must Have

* POS
* Barcode scanning
* Items
* Categories
* Stock
* Batch
* Expiry
* Purchases
* Suppliers
* Customers
* Credit sales
* Customer Khata
* Payments
* Sale returns
* Purchase returns
* Stock corrections
* Basic reports
* Users/permissions
* Receipt printing
* Backup
* Multi-computer support

### Can Follow Later

* WhatsApp receipts
* SMS
* cloud dashboard
* mobile companion app
* advanced accounting
* loyalty points
* online ordering
* supplier online ordering
* multi-branch support

Do not overload V1 with these.

---

# 64. Acceptance Criteria

The product is considered production-ready only when:

### POS

* Barcode sale works without mouse.
* Quantity can be changed quickly.
* Cash calculation is automatic.
* Receipt prints correctly.
* Credit sale works.
* Hold bill works.
* Returns work.

### Inventory

* Stock never becomes inconsistent.
* Every stock change has a reason/source.
* Batch tracking works.
* Expiry tracking works.
* FEFO works.
* Negative stock is controlled.

### Customers

* Walk-in sale requires no customer form.
* Credit customer can be selected quickly.
* Payments update balance immediately.
* Khata history is accurate.

### Suppliers

* Cash purchases work without formal supplier information.
* Supplier credit works.
* Purchase returns update supplier balance.

### Multi-computer

* Two or more POS machines can sell simultaneously.
* Stock remains consistent.
* Customer balances remain consistent.
* No duplicate bill numbers.
* Database locking does not freeze the application unnecessarily.

### Usability

A new cashier should be able to complete:

```text
Scan → Quantity → Pay → Print
```

without technical training.

---

# 65. Final UX Goal

The final software should feel like this:

```text
OPEN SOFTWARE
      ↓
NEW SALE
      ↓
SCAN PRODUCT
      ↓
SCAN NEXT PRODUCT
      ↓
F9
      ↓
CASH
      ↓
ENTER
      ↓
RECEIPT
```

Not:

```text
Open module
→ choose transaction type
→ select entity
→ configure inventory movement
→ choose accounting method
→ create transaction
→ post transaction
→ confirm
→ print
```

The **complexity belongs inside the software architecture, not in front of the pharmacist or cashier.**

For this product, the strongest production architecture is therefore:

> **C++20 + Qt 6 Widgets + PostgreSQL + CMake + native Windows printing/barcode support**, with a reusable component library and a transaction-safe domain/service layer.

The multi-computer requirement is the main reason I would choose PostgreSQL rather than a shared SQLite database. SQLite's WAL mode improves reader/writer concurrency, but it still has one writer and WAL is not designed for separate computers sharing a network filesystem. ([www2.sqlite.org][3])

The SRS should consequently treat **PostgreSQL as the authoritative store database**, while SQLite can remain an optional local technology for future offline/single-computer deployments.

[1]: https://doc.qt.io/qt-6/licensing.html?utm_source=chatgpt.com "Qt Licensing | Qt 6.12"
[2]: https://sqlite.org/faq.html?utm_source=chatgpt.com "SQLite Frequently Asked Questions"
[3]: https://www2.sqlite.org/wal.html?utm_source=chatgpt.com "Write-Ahead Logging"
