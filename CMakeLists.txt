cmake_minimum_required(VERSION 3.20)
project(PakPharmacyPOS VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Qt6 Configuration
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Sql PrintSupport)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Source files
set(SOURCES
    src/main.cpp
    src/app/Application.cpp
    src/app/AppContext.cpp
    src/app/Configuration.cpp
    src/core/Errors.cpp
    src/core/Money.cpp
    src/database/DatabaseManager.cpp
    src/database/Migrations.cpp
    src/services/StockService.cpp
    src/services/SaleService.cpp
    src/services/PurchaseService.cpp
    src/services/LedgerService.cpp
    src/services/ReturnService.cpp
    src/services/BackupService.cpp
    src/ui/Theme.cpp
    src/ui/components/StatusBadge.cpp
    src/ui/components/MoneyInput.cpp
    src/ui/components/QuantityInput.cpp
    src/ui/components/PosSearchBox.cpp
    src/ui/components/DataTable.cpp
    src/ui/components/AppButton.cpp
    src/ui/components/AppTextInput.cpp
    src/ui/components/AppDropdown.cpp
    src/ui/components/AppSearchBox.cpp
    src/ui/components/AppModal.cpp
    src/ui/components/AppToast.cpp
    src/ui/sale/SaleWindow.cpp
    src/ui/sale/PaymentDialog.cpp
    src/ui/sale/HoldBillsDialog.cpp
    src/ui/items/ItemsWindow.cpp
    src/ui/items/QuickAddItemDialog.cpp
    src/ui/stock/StockWindow.cpp
    src/ui/stock/StockCorrectionDialog.cpp
    src/ui/expiry/ExpiryWindow.cpp
    src/ui/purchases/PurchasesWindow.cpp
    src/ui/purchases/CashPurchaseDialog.cpp
    src/ui/customers/CustomersWindow.cpp
    src/ui/customers/ReceivePaymentDialog.cpp
    src/ui/customers/KhataDialog.cpp
    src/ui/suppliers/SuppliersWindow.cpp
    src/ui/returns/SaleReturnDialog.cpp
    src/ui/reports/ReportsWindow.cpp
    src/ui/reports/DayEndClosingDialog.cpp
    src/ui/reports/InvoiceDetailsDialog.cpp
    src/ui/settings/SettingsWindow.cpp
    src/ui/MainWindow.cpp
    src/ui/components/ReceiptPreviewDialog.cpp
    src/printing/ESCPOSPrinter.cpp
    src/printing/ReceiptRenderer.cpp
)

set(HEADERS
    src/app/Application.h
    src/app/AppContext.h
    src/app/Configuration.h
    src/core/Result.h
    src/core/Errors.h
    src/core/Money.h
    src/domain/Models.h
    src/database/DatabaseManager.h
    src/database/Migrations.h
    src/services/StockService.h
    src/services/SaleService.h
    src/services/PurchaseService.h
    src/services/LedgerService.h
    src/services/ReturnService.h
    src/services/BackupService.h
    src/ui/Theme.h
    src/ui/components/StatusBadge.h
    src/ui/components/MoneyInput.h
    src/ui/components/QuantityInput.h
    src/ui/components/PosSearchBox.h
    src/ui/components/DataTable.h
    src/ui/components/AppButton.h
    src/ui/components/AppTextInput.h
    src/ui/components/AppDropdown.h
    src/ui/components/AppSearchBox.h
    src/ui/components/AppModal.h
    src/ui/components/AppToast.h
    src/ui/components/ReceiptPreviewDialog.h
    src/ui/sale/SaleWindow.h
    src/ui/sale/PaymentDialog.h
    src/ui/sale/HoldBillsDialog.h
    src/ui/items/ItemsWindow.h
    src/ui/items/QuickAddItemDialog.h
    src/ui/stock/StockWindow.h
    src/ui/stock/StockCorrectionDialog.h
    src/ui/expiry/ExpiryWindow.h
    src/ui/purchases/PurchasesWindow.h
    src/ui/purchases/CashPurchaseDialog.h
    src/ui/customers/CustomersWindow.h
    src/ui/customers/ReceivePaymentDialog.h
    src/ui/customers/KhataDialog.h
    src/ui/suppliers/SuppliersWindow.h
    src/ui/returns/SaleReturnDialog.h
    src/ui/reports/ReportsWindow.h
    src/ui/reports/DayEndClosingDialog.h
    src/ui/reports/InvoiceDetailsDialog.h
    src/ui/settings/SettingsWindow.h
    src/ui/MainWindow.h
    src/printing/ESCPOSPrinter.h
    src/printing/ReceiptRenderer.h
)

add_executable(PakPharmacyPOS WIN32 ${SOURCES} ${HEADERS})

target_link_libraries(PakPharmacyPOS PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Sql
    Qt6::PrintSupport
)

if (WIN32)
    target_link_libraries(PakPharmacyPOS PRIVATE winspool)
endif()

# Automated Acceptance Test Suite
set(TEST_SOURCES
    tests/test_scenarios.cpp
    src/app/AppContext.cpp
    src/app/Configuration.cpp
    src/core/Errors.cpp
    src/core/Money.cpp
    src/database/DatabaseManager.cpp
    src/database/Migrations.cpp
    src/services/StockService.cpp
    src/services/SaleService.cpp
    src/services/PurchaseService.cpp
    src/services/LedgerService.cpp
    src/services/ReturnService.cpp
    src/services/BackupService.cpp
    src/printing/ESCPOSPrinter.cpp
    src/printing/ReceiptRenderer.cpp
)

add_executable(run_tests ${TEST_SOURCES})

target_link_libraries(run_tests PRIVATE
    Qt6::Core
    Qt6::Sql
    Qt6::PrintSupport
    Qt6::Gui
)

if (WIN32)
    target_link_libraries(run_tests PRIVATE winspool)
endif()

