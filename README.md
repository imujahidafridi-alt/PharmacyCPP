# Pak Pharmacy & Retail POS (Desktop Application)

An enterprise-grade, high-performance Windows desktop POS and inventory management system designed for Pakistani local pharmacies and attached mini-marts.

Built in compliance with the **[srs.md](srs.md)** specification.

---

## Key Highlights

- **Language & GUI:** Modern **C++20** with **Qt 6 Widgets** for ultra-low latency, native Windows keyboard ergonomics, and sub-100ms counter operations.
- **Authoritative Database:** Dual-engine architecture:
  - **PostgreSQL 16+** for multi-computer local store networks (with pessimistic row locks preventing overselling).
  - **SQLite 3 (WAL mode)** for single-PC and resilient local fallback.
- **Counter Workflow (Zero Technical Jargon):**
  - Instant barcode scan & auto-focus loop.
  - Automatic **FEFO (First Expiry First Out)** batch consumption.
  - Multi-tier unit conversions (**Box / Strip / Loose Tablet** math).
  - Built-in **Udhaar / Khata** customer credit ledger and one-click payment collection.
  - **Shah Alam Market Cash Purchases** without requiring complex distributor invoices.
  - **Hold (F7) & Recall (F8)** bills to multitask across concurrent counter customers.
  - Non-destructive returns and audited physical stock corrections.
- **Hardware Integration:**
  - 80mm ESC/POS thermal receipt printing with cash drawer kick pulse.
  - Windows print spooler fallback (`winspool` raw byte writing).
  - Non-blocking print error recovery (printer disconnection does **not** roll back valid sales).

---

## Keyboard Shortcuts (Counter Ergonomics)

| Key | Action |
| :--- | :--- |
| **F2** | Change quantity of selected item in cart |
| **F3** | Focus barcode scanner & item search |
| **F4** | Apply bill or line-item discount |
| **F5** | Select Customer account / Khata balance |
| **F7** | Hold current bill in memory |
| **F8** | Open held bills drawer to resume transactions |
| **F9** | Open Payment dialog (Cash / Udhaar / Change Calc) |
| **Delete** | Remove selected item from cart |
| **Esc** | Cancel / close modal dialogs |

---

## Running the Application

### 1. Quick Launch
Double-click `run_pos.bat` in the root directory:
```cmd
.\run_pos.bat
```

### 2. Run Directly from Build Output
```powershell
.\build\PakPharmacyPOS.exe
```

---

## Running the Automated Test Suite

The project includes an automated acceptance test suite in `tests/test_scenarios.cpp` verifying the core real-world test scenarios from SRS Section 52:
```powershell
.\build\run_tests.exe
```
**Test coverage includes:**
- Scenario 1: Normal medicine sale & atomic stock reduction.
- Scenario 4 & 5: Credit (Udhaar) customer tracking and partial payment reconciliation.
- Scenario 6: Sale return restoring stock and adjusting balance.
- Scenario 7: FEFO batch prioritization (earlier expiry sold first).
- Scenario 10: Automatic cash change calculation.
- Scenario 12: Hold and resume bill workflows.
- Scenario 14 & 15: Cash market purchase and batch creation without supplier.
- Scenario 16 & 17: Audited physical stock mismatch correction.
- Scenario 35: 80mm ESC/POS receipt generation.

---

## Building from Source

### Prerequisites
- CMake 3.25+
- C++20 compiler (`gcc` 13+ or MSVC 2022)
- Qt 6.7+ (Core, Gui, Widgets, Sql, PrintSupport)
- Ninja build tool

### Build Commands
```powershell
# 1. Configure CMake with Ninja
cmake -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release

# 2. Compile Application
cmake --build build --target PakPharmacyPOS

# 3. Deploy Standalone Qt Dependencies
windeployqt6 build\PakPharmacyPOS.exe --dir build
```

---

## Project Architecture

```
src/
├── app/          # Application bootstrap, configuration, and state context
├── core/         # Money (exact integer paisa), Result<T, E>, and non-technical error translators
├── database/     # DatabaseManager, transaction RAII runner, and versioned schema migrations
├── domain/       # Core business models (Item, Batch, CartItem, Sale, Purchase, Ledger)
├── services/     # Transactional services (SaleService, StockService, PurchaseService, LedgerService, ReturnService, BackupService)
├── ui/           # High-contrast Emerald & Slate theme, reusable custom widgets, and module views
└── printing/     # ESC/POS binary command builder and receipt layout renderer
```
