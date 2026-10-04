#include "ui/sale/SaleWindow.h"
#include "ui/sale/PaymentDialog.h"
#include "ui/sale/HoldBillsDialog.h"
#include "ui/sale/CartTableDelegate.h"
#include "services/PriceCalculator.h"
#include "ui/items/QuickAddItemDialog.h"
#include "ui/components/AppToast.h"
#include "services/SaleService.h"
#include "services/StockService.h"
#include "services/LedgerService.h"
#include "printing/ReceiptRenderer.h"
#include "printing/ESCPOSPrinter.h"
#include "database/DatabaseManager.h"
#include "app/AppContext.h"
#include "app/Configuration.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QInputDialog>
#include <QKeyEvent>
#include <QSqlQuery>
#include <cmath>

namespace ui {

SaleWindow::SaleWindow(QWidget* parent) : QWidget(parent)
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 6, 8, 6);
    mainLayout->setSpacing(6);

    // 1. Compact Top Context Row: Bill Chip & Customer Card
    auto* topHeader = new QHBoxLayout();
    topHeader->setContentsMargins(0, 0, 0, 0);
    topHeader->setSpacing(8);
    
    // Bill Chip Card
    auto* billCard = new QFrame(this);
    billCard->setStyleSheet("background-color: #FFFFFF; border: 1px solid #CBD5E1; border-radius: 3px; padding: 2px 8px;");
    auto* billLayout = new QHBoxLayout(billCard);
    billLayout->setContentsMargins(4, 2, 4, 2);
    billLayout->setSpacing(6);

    auto* billTitle = new QLabel("Bill:", billCard);
    billTitle->setStyleSheet("font-size: 11px; font-weight: 700; color: #64748B;");
    m_billNumberLabel = new QLabel(billCard);
    m_billNumberLabel->setStyleSheet("font-size: 12px; font-weight: 700; color: #0F766E;");

    billLayout->addWidget(billTitle);
    billLayout->addWidget(m_billNumberLabel);
    topHeader->addWidget(billCard);

    topHeader->addStretch();

    // Customer Card
    m_customerCard = new QFrame(this);
    m_customerCard->setStyleSheet("background-color: #FFFFFF; border: 1px solid #CBD5E1; border-radius: 3px; padding: 2px 8px;");
    m_customerCard->setCursor(Qt::PointingHandCursor);
    auto* custLayout = new QHBoxLayout(m_customerCard);
    custLayout->setContentsMargins(4, 2, 4, 2);
    custLayout->setSpacing(6);

    auto* custTitle = new QLabel("Customer:", m_customerCard);
    custTitle->setStyleSheet("font-size: 11px; font-weight: 700; color: #64748B;");
    m_customerNameLabel = new QLabel("Walk-in Customer", m_customerCard);
    m_customerNameLabel->setStyleSheet("font-size: 12px; font-weight: 600; color: #1E293B;");

    m_customerBaqayaBadge = new QLabel(m_customerCard);
    m_customerBaqayaBadge->setVisible(false);

    auto* changeHint = new QLabel("[F5]", m_customerCard);
    changeHint->setStyleSheet("color: #0F766E; font-size: 11px; font-weight: 700;");

    custLayout->addWidget(custTitle);
    custLayout->addWidget(m_customerNameLabel);
    custLayout->addWidget(m_customerBaqayaBadge);
    custLayout->addWidget(changeHint);

    topHeader->addWidget(m_customerCard);
    mainLayout->addLayout(topHeader);

    // 2. Compact Search Bar with Regex & Power-Typing commands
    m_searchBox = new PosSearchBox(this);
    mainLayout->addWidget(m_searchBox);

    // 3. Middle Area: Cart Table + Mutabadil Side Drawer
    auto* centerArea = new QHBoxLayout();
    centerArea->setSpacing(6);
    centerArea->setContentsMargins(0, 0, 0, 0);

    auto* tableContainer = new QWidget(this);
    auto* tableLayout = new QGridLayout(tableContainer);
    tableLayout->setContentsMargins(0, 0, 0, 0);

    m_cartTable = new DataTable(tableContainer);
    m_cartTable->setupHeaders({"#", "ITEM NAME", "UNIT", "BATCH", "EXPIRY", "QTY (F2)", "MRP", "DISC %", "SALE PRICE", "TOTAL"});
    m_cartTable->horizontalHeader()->setStretchLastSection(false);
    m_cartTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);

    // Synchronize header text alignments with column data alignments
    for (int i = 0; i < 10; ++i) {
        if (auto* h = m_cartTable->horizontalHeaderItem(i)) {
            if (i == 1) {
                h->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            } else if (i == 6 || i == 8 || i == 9) {
                h->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            } else {
                h->setTextAlignment(Qt::AlignCenter);
            }
        }
    }

    // Readable, spacious column widths
    m_cartTable->setColumnWidth(0, 40);  // #
    m_cartTable->setColumnWidth(2, 75);  // UNIT
    m_cartTable->setColumnWidth(3, 95);  // BATCH
    m_cartTable->setColumnWidth(4, 105); // EXPIRY
    m_cartTable->setColumnWidth(5, 85);  // QTY (F2) inline editor
    m_cartTable->setColumnWidth(6, 85);  // MRP
    m_cartTable->setColumnWidth(7, 80);  // DISC % inline editor
    m_cartTable->setColumnWidth(8, 100); // SALE PRICE inline editor
    m_cartTable->setColumnWidth(9, 115); // TOTAL

    // Install custom inline editor delegate on Qty (5), Disc % (7), and Sale Price (8) columns
    auto* cartDelegate = new CartTableDelegate([this]() -> const std::vector<domain::CartItem>& {
        return m_cart;
    }, this);
    m_cartTable->setItemDelegateForColumn(5, cartDelegate);
    m_cartTable->setItemDelegateForColumn(7, cartDelegate);
    m_cartTable->setItemDelegateForColumn(8, cartDelegate);
    m_cartTable->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed | QAbstractItemView::SelectedClicked);
    m_cartTable->installEventFilter(this);

    connect(cartDelegate, &CartTableDelegate::nonDiscountableBlocked, this, [this](const QString& itemName) {
        AppToast::showWarning(this, QString("🔒 '%1' is Non-Discountable (FMCG / Regulated MRP).").arg(itemName));
    });
    connect(cartDelegate, &QAbstractItemDelegate::closeEditor, this, [this]() {
        m_searchBox->setFocus();
    });

    // Empty State Overlay Card
    m_emptyStateOverlay = new QWidget(tableContainer);
    m_emptyStateOverlay->setStyleSheet("background-color: transparent;");
    auto* emptyLayout = new QVBoxLayout(m_emptyStateOverlay);
    emptyLayout->setAlignment(Qt::AlignCenter);
    emptyLayout->setSpacing(4);

    auto* emptyIcon = new QLabel("🛒", m_emptyStateOverlay);
    emptyIcon->setAlignment(Qt::AlignCenter);
    emptyIcon->setStyleSheet("font-size: 28px; color: #CBD5E1;");

    auto* emptyTitle = new QLabel("Ready to Scan Items", m_emptyStateOverlay);
    emptyTitle->setAlignment(Qt::AlignCenter);
    emptyTitle->setStyleSheet("font-size: 13px; font-weight: 700; color: #64748B;");

    auto* emptyDesc = new QLabel("Scan barcode or type 5*Panadol / -Wapsi / Alt+M for Mutabadil alternatives.", m_emptyStateOverlay);
    emptyDesc->setAlignment(Qt::AlignCenter);
    emptyDesc->setStyleSheet("font-size: 11px; color: #94A3B8;");

    emptyLayout->addWidget(emptyIcon);
    emptyLayout->addWidget(emptyTitle);
    emptyLayout->addWidget(emptyDesc);

    tableLayout->addWidget(m_cartTable, 0, 0);
    tableLayout->addWidget(m_emptyStateOverlay, 0, 0);
    centerArea->addWidget(tableContainer, 1);

    // Mutabadil Generic Substitute Side Drawer (Starts Collapsed)
    m_mutabadilDrawer = new QFrame(this);
    m_mutabadilDrawer->setFixedWidth(330);
    m_mutabadilDrawer->setStyleSheet(
        "QFrame { background-color: #F8FAFC; border: 1.5px solid #0F766E; border-radius: 4px; }"
    );
    m_mutabadilDrawer->setVisible(false);

    auto* drawerLayout = new QVBoxLayout(m_mutabadilDrawer);
    drawerLayout->setContentsMargins(8, 8, 8, 8);
    drawerLayout->setSpacing(6);

    auto* drawerHeader = new QHBoxLayout();
    auto* drawerTitle = new QLabel("MUTABADIL ALTERNATIVES", m_mutabadilDrawer);
    drawerTitle->setStyleSheet("font-size: 11px; font-weight: 800; color: #0F766E; letter-spacing: 0.5px; border: none; background: transparent;");

    auto* closeDrawerBtn = new QPushButton("✕", m_mutabadilDrawer);
    closeDrawerBtn->setFixedSize(18, 18);
    closeDrawerBtn->setCursor(Qt::PointingHandCursor);
    closeDrawerBtn->setStyleSheet("QPushButton { border: none; font-weight: 800; color: #64748B; background: transparent; } QPushButton:hover { color: #DC2626; }");
    connect(closeDrawerBtn, &QPushButton::clicked, this, [this]() {
        m_mutabadilDrawer->setVisible(false);
        m_searchBox->setFocus();
    });

    drawerHeader->addWidget(drawerTitle);
    drawerHeader->addStretch();
    drawerHeader->addWidget(closeDrawerBtn);
    drawerLayout->addLayout(drawerHeader);

    m_mutabadilTitleLabel = new QLabel(m_mutabadilDrawer);
    m_mutabadilTitleLabel->setStyleSheet("font-size: 11px; color: #334155; font-weight: 600; border: none; background: transparent;");
    m_mutabadilTitleLabel->setWordWrap(true);
    drawerLayout->addWidget(m_mutabadilTitleLabel);

    m_mutabadilTable = new DataTable(m_mutabadilDrawer);
    m_mutabadilTable->setupHeaders({"Brand", "Price", "Savings", "Stock"});
    m_mutabadilTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_mutabadilTable->setColumnWidth(1, 65);
    m_mutabadilTable->setColumnWidth(2, 70);
    m_mutabadilTable->setColumnWidth(3, 45);
    drawerLayout->addWidget(m_mutabadilTable, 1);

    auto* drawerHint = new QLabel("Double-click or press Enter to substitute into cart.", m_mutabadilDrawer);
    drawerHint->setStyleSheet("font-size: 10px; color: #64748B; font-style: italic; border: none; background: transparent;");
    drawerLayout->addWidget(drawerHint);

    centerArea->addWidget(m_mutabadilDrawer);
    mainLayout->addLayout(centerArea, 1);

    // 4. Compact Summary Strip
    auto* footerCard = new QFrame(this);
    footerCard->setObjectName("totalDisplayCard");
    auto* footerLayout = new QHBoxLayout(footerCard);
    footerLayout->setContentsMargins(10, 4, 10, 4);
    footerLayout->setSpacing(12);

    m_itemCountLabel = new QLabel("Lines: 0 | Qty: 0", footerCard);
    m_itemCountLabel->setStyleSheet("font-size: 12px; font-weight: 700; color: #334155;");

    m_subtotalLabel = new QLabel("Subtotal: Rs. 0", footerCard);
    m_subtotalLabel->setStyleSheet("font-size: 12px; font-weight: 600; color: #64748B;");

    m_discountLabel = new QLabel("Discount: Rs. 0", footerCard);
    m_discountLabel->setStyleSheet("font-size: 12px; font-weight: 600; color: #64748B;");

    footerLayout->addWidget(m_itemCountLabel);
    footerLayout->addSpacing(8);
    footerLayout->addWidget(m_subtotalLabel);
    footerLayout->addSpacing(8);
    footerLayout->addWidget(m_discountLabel);
    footerLayout->addStretch();

    auto* totTitle = new QLabel("TOTAL:", footerCard);
    totTitle->setStyleSheet("font-size: 12px; font-weight: 700; color: #64748B;");
    
    m_totalLabel = new QLabel("Rs. 0", footerCard);
    m_totalLabel->setObjectName("totalDisplayAmount");

    footerLayout->addWidget(totTitle);
    footerLayout->addWidget(m_totalLabel);

    mainLayout->addWidget(footerCard);

    // 5. Flat, Compact Shortcut Buttons (Single Row for Small Screens)
    auto* shortcutBar = new QHBoxLayout();
    shortcutBar->setContentsMargins(0, 0, 0, 0);
    shortcutBar->setSpacing(4);

    auto addFlatBtn = [this, shortcutBar](const QString& text, auto slot) -> QPushButton* {
        auto* btn = new QPushButton(text, this);
        btn->setProperty("class", "secondaryBtn");
        btn->setFixedHeight(26);
        btn->setAutoDefault(false);
        btn->setDefault(false);
        connect(btn, &QPushButton::clicked, this, slot);
        shortcutBar->addWidget(btn);
        return btn;
    };

    addFlatBtn("F2 Quantity", &SaleWindow::handleQuantityShortcut);
    addFlatBtn("F4 Discount", &SaleWindow::handleDiscountShortcut);
    addFlatBtn("F5 Customer", &SaleWindow::handleCustomerShortcut);
    addFlatBtn("F6 Unit (U)", &SaleWindow::handleUnitToggleShortcut);
    addFlatBtn("F7 Hold", &SaleWindow::handleHoldShortcut);
    m_heldBillsBtn = addFlatBtn("F8 Held (0)", &SaleWindow::handleHeldBillsShortcut);
    m_mutabadilBtn = addFlatBtn("Alt+M Mutabadil", &SaleWindow::handleMutabadilShortcut);
    addFlatBtn("Alt+R Round 5", &SaleWindow::handleChillarRoundShortcut);
    
    auto* removeBtn = addFlatBtn("Del Remove", &SaleWindow::handleRemoveSelectedItem);
    removeBtn->setProperty("class", "dangerBtn");

    auto* payBtn = new QPushButton("Pay && Print (F9)", this);
    payBtn->setObjectName("primaryCheckoutBtn");
    payBtn->setFixedHeight(26);
    payBtn->setAutoDefault(false);
    payBtn->setDefault(false);
    connect(payBtn, &QPushButton::clicked, this, &SaleWindow::handleCheckoutShortcut);
    shortcutBar->addWidget(payBtn);

    mainLayout->addLayout(shortcutBar);

    // Direct item selection from multi-column live suggestion popup
    connect(m_searchBox, &PosSearchBox::itemSelected, this, &SaleWindow::handleItemSelected);
    // Single authoritative connection for direct barcode / text commands
    connect(m_searchBox, &PosSearchBox::commandEntered, this, &SaleWindow::handleCommandEntered);
    connect(m_searchBox, &PosSearchBox::navigateToCartRequested, this, [this]() {
        if (m_cartTable->rowCount() > 0) {
            m_cartTable->setFocus();
            if (m_cartTable->currentRow() < 0) {
                m_cartTable->selectRow(0);
            }
        }
    });

    connect(m_cartTable, &QTableWidget::cellChanged, this, &SaleWindow::handleCellChanged);

    connect(m_cartTable, &QTableWidget::cellDoubleClicked, this, [this](int row, int col) {
        if (row < 0 || row >= static_cast<int>(m_cart.size())) return;
        if (col == 2) {
            handleUnitToggleShortcut();
        } else if ((col == 7 || col == 8) && !m_cart[row].isDiscountable) {
            AppToast::showWarning(this, QString("🔒 '%1' is Non-Discountable (FMCG / MRP Locked).").arg(m_cart[row].itemName));
        }
    });

    connect(m_mutabadilTable, &QTableWidget::cellDoubleClicked, this, [this](int row, int /*col*/) {
        if (row < 0 || row >= m_mutabadilTable->rowCount()) return;
        int subItemId = m_mutabadilTable->item(row, 0)->data(Qt::UserRole).toInt();
        if (subItemId > 0) {
            auto& dbMgr = database::DatabaseManager::instance();
            QSqlDatabase db = dbMgr.connection();
            QSqlQuery q(db);
            q.prepare(R"(
                SELECT i.*, c.name as category_name,
                       COALESCE(c.is_discountable, 1) as cat_is_discountable,
                       COALESCE(c.default_disc_pct, 0.0) as cat_default_disc_pct,
                       COALESCE(c.max_discount_pct, 15.0) as cat_max_disc_pct
                FROM items i
                LEFT JOIN categories c ON i.category_id = c.id
                WHERE i.id = ?
            )");
            q.addBindValue(subItemId);
            if (q.exec() && q.next()) {
                domain::Item subItem;
                subItem.id = q.value("id").toInt();
                subItem.code = q.value("code").toString();
                subItem.name = q.value("name").toString();
                subItem.categoryId = q.value("category_id").toInt();
                subItem.categoryName = q.value("category_name").toString();
                subItem.brand = q.value("brand").toString();
                subItem.salePrice = core::Money::fromPaisa(q.value("sale_price_paisa").toLongLong());
                subItem.purchaseCost = core::Money::fromPaisa(q.value("purchase_cost_paisa").toLongLong());
                int64_t tpVal = q.value("tp_paisa").toLongLong();
                subItem.tp = (tpVal > 0) ? core::Money::fromPaisa(tpVal) : subItem.purchaseCost;
                subItem.isMedicine = (q.value("is_medicine").toInt() == 1);
                subItem.piecesPerStrip = q.value("pieces_per_strip").toInt();
                subItem.stripsPerBox = q.value("strips_per_box").toInt();
                subItem.stripSalePrice = core::Money::fromPaisa(q.value("strip_sale_price_paisa").toLongLong());
                subItem.boxSalePrice = core::Money::fromPaisa(q.value("box_sale_price_paisa").toLongLong());

                subItem.categoryDiscountable = (q.value("cat_is_discountable").toInt() == 1);
                subItem.categoryDefaultDiscountPct = q.value("cat_default_disc_pct").toDouble();
                subItem.categoryMaxDiscountPct = q.value("cat_max_disc_pct").toDouble();
                if (subItem.categoryMaxDiscountPct <= 0.0 && subItem.categoryDiscountable) {
                    subItem.categoryMaxDiscountPct = 15.0;
                }
                if (!q.value("item_is_discountable").isNull()) {
                    subItem.isDiscountableOverride = (q.value("item_is_discountable").toInt() == 1);
                }
                if (!q.value("override_disc_pct").isNull()) {
                    subItem.discountPctOverride = q.value("override_disc_pct").toDouble();
                }
                subItem.minMarginPct = q.value("min_margin_pct").toDouble();

                addItemToCart(subItem);
                AppToast::showSuccess(this, QString("Substituted '%1' into cart!").arg(subItem.name));
                m_mutabadilDrawer->setVisible(false);
                m_searchBox->setFocus();
            }
        }
    });

    resetSale();
}

void SaleWindow::resetSale()
{
    m_cart.clear();
    m_discount = core::Money(0);
    
    m_currentCustomer.id = 1;
    m_currentCustomer.name = "Walk-in Customer";
    m_currentCustomer.baqaya = core::Money(0);

    m_billNumberLabel->setText(services::SaleService::instance().generateNextBillNumber());
    updateCustomerDisplay();

    m_cartTable->blockSignals(true);
    m_cartTable->setRowCount(0);
    m_cartTable->blockSignals(false);

    m_mutabadilDrawer->setVisible(false);

    updateTotals();
    updateEmptyState();
    m_searchBox->setFocus();
}

void SaleWindow::updateCustomerDisplay()
{
    m_customerNameLabel->setText(m_currentCustomer.name);
    if (m_currentCustomer.baqaya.isPositive()) {
        m_customerBaqayaBadge->setText(QString("Baqaya: %1").arg(m_currentCustomer.baqaya.formatted()));
        m_customerBaqayaBadge->setStyleSheet("background-color: #FEE2E2; color: #DC2626; font-size: 11px; font-weight: 700; padding: 1px 6px; border-radius: 2px;");
        m_customerBaqayaBadge->setVisible(true);
    } else {
        m_customerBaqayaBadge->setVisible(false);
    }
}

void SaleWindow::updateEmptyState()
{
    bool empty = m_cart.empty();
    m_emptyStateOverlay->setVisible(empty);

    int heldCount = static_cast<int>(services::SaleService::instance().getHeldBills().size());
    m_heldBillsBtn->setText(QString("F8 Held (%1)").arg(heldCount));
}

void SaleWindow::handleItemSelected(const domain::Item& item, int qty, domain::SaleUnitSelection unit)
{
    addItemToCart(item, qty, unit);
    if (qty < 0) {
        AppToast::showInfo(this, QString("Added WAPSI return: %1 (Qty: %2)").arg(item.name).arg(qty));
    }
    m_searchBox->clear();
    m_searchBox->setFocus();
}

void SaleWindow::handleCommandEntered(const ui::PosSearchBox::ParsedCommand& cmd)
{
    if (cmd.query.trimmed().isEmpty()) return;

    auto& dbMgr = database::DatabaseManager::instance();
    QSqlDatabase db = dbMgr.connection();
    if (!db.isOpen()) return;

    QSqlQuery q(db);
    bool found = false;

    if (cmd.isBarcode) {
        q.prepare(R"(
            SELECT i.*, c.name as category_name,
                   COALESCE(c.is_discountable, 1) as cat_is_discountable,
                   COALESCE(c.default_disc_pct, 0.0) as cat_default_disc_pct,
                   COALESCE(c.max_discount_pct, 15.0) as cat_max_disc_pct
            FROM items i
            LEFT JOIN categories c ON i.category_id = c.id
            LEFT JOIN item_barcodes ib ON i.id = ib.item_id
            WHERE (i.barcode = ? OR ib.barcode = ? OR i.code = ?) AND i.is_active = 1
            LIMIT 1
        )");
        q.addBindValue(cmd.query);
        q.addBindValue(cmd.query);
        q.addBindValue(cmd.query);
        found = (q.exec() && q.next());
    } else {
        // 1. Try exact name or code match first (essential when selected from dropdown!)
        q.prepare(R"(
            SELECT i.*, c.name as category_name,
                   COALESCE(c.is_discountable, 1) as cat_is_discountable,
                   COALESCE(c.default_disc_pct, 0.0) as cat_default_disc_pct,
                   COALESCE(c.max_discount_pct, 15.0) as cat_max_disc_pct
            FROM items i
            LEFT JOIN categories c ON i.category_id = c.id
            WHERE (i.name = ? OR i.code = ?) AND i.is_active = 1
            LIMIT 1
        )");
        q.addBindValue(cmd.query);
        q.addBindValue(cmd.query);
        found = (q.exec() && q.next());

        if (!found) {
            // 2. Fallback to fuzzy search
            q.prepare(R"(
                SELECT i.*, c.name as category_name,
                       COALESCE(c.is_discountable, 1) as cat_is_discountable,
                       COALESCE(c.default_disc_pct, 0.0) as cat_default_disc_pct,
                       COALESCE(c.max_discount_pct, 15.0) as cat_max_disc_pct
                FROM items i
                LEFT JOIN categories c ON i.category_id = c.id
                WHERE (i.name LIKE ? OR i.code LIKE ? OR i.generic_name LIKE ?) AND i.is_active = 1
                LIMIT 1
            )");
            QString wildcard = "%" + cmd.query + "%";
            q.addBindValue(wildcard);
            q.addBindValue(wildcard);
            q.addBindValue(wildcard);
            found = (q.exec() && q.next());
        }
    }

    if (found) {
        domain::Item item;
        item.id = q.value("id").toInt();
        item.code = q.value("code").toString();
        item.name = q.value("name").toString();
        item.categoryId = q.value("category_id").toInt();
        item.categoryName = q.value("category_name").toString();
        item.brand = q.value("brand").toString();
        item.genericName = q.value("generic_name").toString();
        item.barcode = q.value("barcode").toString();
        item.salePrice = core::Money::fromPaisa(q.value("sale_price_paisa").toLongLong());
        item.purchaseCost = core::Money::fromPaisa(q.value("purchase_cost_paisa").toLongLong());
        int64_t tpVal = q.value("tp_paisa").toLongLong();
        item.tp = (tpVal > 0) ? core::Money::fromPaisa(tpVal) : item.purchaseCost;
        item.isMedicine = (q.value("is_medicine").toInt() == 1);
        item.piecesPerStrip = q.value("pieces_per_strip").toInt();
        item.stripsPerBox = q.value("strips_per_box").toInt();
        item.stripSalePrice = core::Money::fromPaisa(q.value("strip_sale_price_paisa").toLongLong());
        item.boxSalePrice = core::Money::fromPaisa(q.value("box_sale_price_paisa").toLongLong());

        item.categoryDiscountable = (q.value("cat_is_discountable").toInt() == 1);
        item.categoryDefaultDiscountPct = q.value("cat_default_disc_pct").toDouble();
        item.categoryMaxDiscountPct = q.value("cat_max_disc_pct").toDouble();
        if (item.categoryMaxDiscountPct <= 0.0 && item.categoryDiscountable) {
            item.categoryMaxDiscountPct = 15.0;
        }
        if (!q.value("item_is_discountable").isNull()) {
            item.isDiscountableOverride = (q.value("item_is_discountable").toInt() == 1);
        }
        if (!q.value("override_disc_pct").isNull()) {
            item.discountPctOverride = q.value("override_disc_pct").toDouble();
        }
        item.minMarginPct = q.value("min_margin_pct").toDouble();

        domain::SaleUnitSelection unit = cmd.hasExplicitUnit ? cmd.unit : domain::SaleUnitSelection::PieceOrTablet;
        addItemToCart(item, cmd.qty, unit);

        if (cmd.isReturn) {
            AppToast::showInfo(this, QString("Added WAPSI return: %1 (Qty: %2)").arg(item.name).arg(cmd.qty));
        }
    } else {
        promptAddItem(cmd.query);
    }

    m_searchBox->clear();
    m_searchBox->setFocus();
}

void SaleWindow::handleBarcodeEntered(const QString& barcode)
{
    PosSearchBox::ParsedCommand cmd;
    cmd.query = barcode;
    cmd.isBarcode = true;
    cmd.qty = 1;
    handleCommandEntered(cmd);
}

void SaleWindow::handleSearchEntered(const QString& text)
{
    PosSearchBox::ParsedCommand cmd;
    cmd.query = text;
    cmd.isBarcode = false;
    cmd.qty = 1;
    handleCommandEntered(cmd);
}

void SaleWindow::addItemToCart(const domain::Item& item, int qty, domain::SaleUnitSelection unit)
{
    core::Money uMrp = item.salePrice;
    core::Money uTp = item.tp.isPositive() ? item.tp : item.purchaseCost;
    int atomicPerUnit = 1;
    QString unitStr = item.isMedicine ? "Tablet" : "Piece";

    if (unit == domain::SaleUnitSelection::Box && item.stripsPerBox > 1) {
        atomicPerUnit = item.stripsPerBox * item.piecesPerStrip;
        uMrp = item.boxSalePrice.isPositive() ? item.boxSalePrice : (item.salePrice * atomicPerUnit);
        uTp = (item.tp.isPositive() ? item.tp : item.purchaseCost) * atomicPerUnit;
        unitStr = "Box";
    } else if (unit == domain::SaleUnitSelection::Strip && item.piecesPerStrip > 1) {
        atomicPerUnit = item.piecesPerStrip;
        uMrp = item.stripSalePrice.isPositive() ? item.stripSalePrice : (item.salePrice * atomicPerUnit);
        uTp = (item.tp.isPositive() ? item.tp : item.purchaseCost) * atomicPerUnit;
        unitStr = "Strip";
    }

    // Don't merge returns into regular positive lines
    for (size_t i = 0; i < m_cart.size(); ++i) {
        if (m_cart[i].itemId == item.id && m_cart[i].unitSelection == unit && ((m_cart[i].displayQty > 0 && qty > 0) || (m_cart[i].displayQty < 0 && qty < 0))) {
            m_cart[i].displayQty += qty;
            m_cart[i].totalAtomicQty = m_cart[i].displayQty * m_cart[i].atomicUnitsPerQty;

            auto comp = services::PriceCalculator::calculateRowTotals(
                item,
                item.categoryDiscountable,
                item.categoryDefaultDiscountPct,
                item.categoryMaxDiscountPct,
                m_cart[i].displayQty,
                m_cart[i].discountPct,
                m_cart[i].unitMrp,
                m_cart[i].unitTp
            );
            m_cart[i].totalGross = comp.totalGross;
            m_cart[i].totalDiscount = comp.totalDiscount;
            m_cart[i].totalAmount = comp.totalAmount;

            updateTableRow(static_cast<int>(i));
            updateTotals();
            return;
        }
    }

    auto comp = services::PriceCalculator::calculateRowTotals(
        item,
        item.categoryDiscountable,
        item.categoryDefaultDiscountPct,
        item.categoryMaxDiscountPct,
        qty,
        -1.0, // Inherit default discount
        uMrp,
        uTp
    );

    auto fefoRes = services::StockService::instance().allocateFefo(item.id, std::abs(qty) * atomicPerUnit);
    domain::CartItem cartItem;
    cartItem.itemId = item.id;
    cartItem.itemName = (qty < 0) ? (item.name + " [WAPSI / RETURN]") : item.name;
    cartItem.barcode = item.barcode;
    cartItem.unitSelection = unit;
    cartItem.displayQty = qty;
    cartItem.atomicUnitsPerQty = atomicPerUnit;
    cartItem.totalAtomicQty = qty * atomicPerUnit;

    cartItem.unitMrp = comp.unitMrp;
    cartItem.unitTp = comp.unitTp;
    cartItem.isDiscountable = comp.isDiscountable;
    cartItem.discountPct = comp.discountPct;
    cartItem.unitDiscount = comp.unitDiscount;
    cartItem.unitSalePrice = comp.unitSalePrice;
    cartItem.unitPrice = comp.unitSalePrice;
    cartItem.totalGross = comp.totalGross;
    cartItem.totalDiscount = comp.totalDiscount;
    cartItem.totalAmount = comp.totalAmount;

    if (fefoRes.isOk() && !fefoRes.value().empty()) {
        const auto& alloc = fefoRes.value().front();
        cartItem.batchId = alloc.batchId;
        cartItem.batchNumber = alloc.batchNumber;
        cartItem.expiryDate = alloc.expiryDate;
    }

    m_cart.push_back(cartItem);

    int r = m_cartTable->rowCount();
    m_cartTable->blockSignals(true);
    m_cartTable->insertRow(r);

    auto makeItem = [](const QString& text, Qt::Alignment align = Qt::AlignLeft, bool editable = false) {
        auto* it = new QTableWidgetItem(text);
        it->setTextAlignment(align | Qt::AlignVCenter);
        if (editable) {
            it->setFlags(it->flags() | Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        } else {
            it->setFlags(it->flags() & ~Qt::ItemIsEditable);
        }
        return it;
    };

    // Col 0: #
    m_cartTable->setItem(r, 0, makeItem(QString::number(r + 1), Qt::AlignCenter));

    // Col 1: ITEM NAME (Bold, crisp dark slate)
    auto* nameItem = makeItem(cartItem.itemName, Qt::AlignLeft);
    QFont nameFont = nameItem->font();
    nameFont.setBold(true);
    nameItem->setFont(nameFont);
    nameItem->setForeground(QBrush(QColor("#0F172A")));
    m_cartTable->setItem(r, 1, nameItem);

    // Col 2: UNIT
    m_cartTable->setItem(r, 2, makeItem(unitStr, Qt::AlignCenter));

    // Col 3: BATCH
    auto* batchItem = makeItem(cartItem.batchNumber, Qt::AlignCenter);
    m_cartTable->setItem(r, 3, batchItem);

    // Col 4: EXPIRY with FEFO visual alert
    QString expiryStr = cartItem.expiryDate.isValid() ? cartItem.expiryDate.toString("MM/yyyy") : "-";
    auto* expItem = makeItem(expiryStr, Qt::AlignCenter);

    if (cartItem.expiryDate.isValid()) {
        int daysLeft = QDate::currentDate().daysTo(cartItem.expiryDate);
        if (daysLeft <= 0) {
            expItem->setText(expiryStr + " ⚠ EXPIRED");
            expItem->setBackground(QBrush(QColor("#FEE2E2")));
            expItem->setForeground(QBrush(QColor("#DC2626")));
            batchItem->setBackground(QBrush(QColor("#FEE2E2")));
            batchItem->setForeground(QBrush(QColor("#DC2626")));
            AppToast::showWarning(this, QString("⚠ WARNING: %1 is EXPIRED (Batch %2)!").arg(cartItem.itemName, cartItem.batchNumber));
        } else if (daysLeft <= 90) {
            expItem->setText(QString("%1 (%2d left)").arg(expiryStr).arg(daysLeft));
            expItem->setBackground(QBrush(QColor("#FEF3C7")));
            expItem->setForeground(QBrush(QColor("#B45309")));
            batchItem->setBackground(QBrush(QColor("#FEF3C7")));
            batchItem->setForeground(QBrush(QColor("#B45309")));
        }
    }
    m_cartTable->setItem(r, 4, expItem);

    // Col 5: QTY (editable, bold)
    auto* qtyItem = makeItem(QString::number(qty), Qt::AlignCenter, true);
    QFont qtyFont = qtyItem->font();
    qtyFont.setBold(true);
    qtyItem->setFont(qtyFont);
    m_cartTable->setItem(r, 5, qtyItem);

    // Col 6: MRP
    m_cartTable->setItem(r, 6, makeItem(cartItem.unitMrp.formatted(false), Qt::AlignRight));

    // Col 7: DISC %
    QString discStr = cartItem.isDiscountable
        ? (QString::number(cartItem.discountPct, 'f', (std::fmod(cartItem.discountPct, 1.0) == 0.0 ? 0 : 2)) + "%")
        : "🔒 0%";
    auto* discItem = makeItem(discStr, Qt::AlignCenter, cartItem.isDiscountable);
    if (!cartItem.isDiscountable) {
        discItem->setForeground(QBrush(QColor("#64748B")));
    } else if (cartItem.discountPct > 0) {
        discItem->setForeground(QBrush(QColor("#15803D")));
        QFont discFont = discItem->font();
        discFont.setBold(true);
        discItem->setFont(discFont);
    }
    m_cartTable->setItem(r, 7, discItem);

    // Col 8: SALE PRICE
    auto* salePriceItem = makeItem(cartItem.unitSalePrice.formatted(false), Qt::AlignRight, cartItem.isDiscountable);
    if (!cartItem.isDiscountable) {
        salePriceItem->setForeground(QBrush(QColor("#64748B")));
    } else {
        QFont spFont = salePriceItem->font();
        spFont.setBold(true);
        salePriceItem->setFont(spFont);
    }
    m_cartTable->setItem(r, 8, salePriceItem);

    // Col 9: TOTAL
    auto* totalItem = makeItem(cartItem.totalAmount.formatted(), Qt::AlignRight);
    QFont totalFont = totalItem->font();
    totalFont.setBold(true);
    totalItem->setFont(totalFont);
    totalItem->setForeground(QBrush(QColor("#0F766E")));
    m_cartTable->setItem(r, 9, totalItem);

    if (qty < 0) {
        // Wapsi Row styling
        for (int c = 0; c < 10; ++c) {
            if (c != 5 && c != 7 && c != 8) {
                auto* it = m_cartTable->item(r, c);
                if (it && c != 4) it->setBackground(QBrush(QColor("#EFF6FF")));
            }
        }
        m_cartTable->item(r, 1)->setForeground(QBrush(QColor("#1D4ED8")));
        m_cartTable->item(r, 9)->setForeground(QBrush(QColor("#DC2626")));
    }

    m_cartTable->blockSignals(false);
    m_cartTable->selectRow(r);

    if (comp.wasClampedDueToLoss) {
        AppToast::showWarning(this, QString("⚠ Margin Protection: %1 discount clamped to preserve Trade Price.").arg(item.name));
    }

    updateTotals();
    updateEmptyState();
}

void SaleWindow::updateTableRow(int row)
{
    if (row < 0 || row >= static_cast<int>(m_cart.size()) || row >= m_cartTable->rowCount()) return;
    const auto& cartItem = m_cart[row];

    m_cartTable->blockSignals(true);

    // Col 0: #
    m_cartTable->item(row, 0)->setText(QString::number(row + 1));

    // Col 1: ITEM NAME (Bold)
    auto* nameItem = m_cartTable->item(row, 1);
    nameItem->setText(cartItem.itemName);
    QFont nameFont = nameItem->font();
    nameFont.setBold(true);
    nameItem->setFont(nameFont);

    // Col 2: UNIT
    QString unitStr = "Piece";
    if (cartItem.unitSelection == domain::SaleUnitSelection::Box) unitStr = "Box";
    else if (cartItem.unitSelection == domain::SaleUnitSelection::Strip) unitStr = "Strip";
    else unitStr = "Tablet";
    m_cartTable->item(row, 2)->setText(unitStr);

    // Col 3: BATCH
    m_cartTable->item(row, 3)->setText(cartItem.batchNumber);

    // Col 4: EXPIRY
    QString expiryStr = cartItem.expiryDate.isValid() ? cartItem.expiryDate.toString("MM/yyyy") : "-";
    if (cartItem.expiryDate.isValid()) {
        int daysLeft = QDate::currentDate().daysTo(cartItem.expiryDate);
        if (daysLeft <= 0) {
            expiryStr += " ⚠ EXPIRED";
        } else if (daysLeft <= 90) {
            expiryStr += QString(" (%1d left)").arg(daysLeft);
        }
    }
    m_cartTable->item(row, 4)->setText(expiryStr);

    // Col 5: QTY
    m_cartTable->item(row, 5)->setText(QString::number(cartItem.displayQty));

    // Col 6: MRP
    m_cartTable->item(row, 6)->setText(cartItem.unitMrp.formatted(false));

    // Col 7: DISC %
    auto* discItem = m_cartTable->item(row, 7);
    if (!cartItem.isDiscountable) {
        discItem->setText("🔒 0%");
        discItem->setForeground(QBrush(QColor("#64748B")));
        discItem->setFlags(discItem->flags() & ~Qt::ItemIsEditable);
    } else {
        QString pctStr = QString::number(cartItem.discountPct, 'f', (std::fmod(cartItem.discountPct, 1.0) == 0.0 ? 0 : 2)) + "%";
        discItem->setText(pctStr);
        discItem->setForeground(cartItem.discountPct > 0 ? QBrush(QColor("#15803D")) : QBrush(QColor("#1E293B")));
        discItem->setFlags(discItem->flags() | Qt::ItemIsEditable);
        if (cartItem.discountPct > 0) {
            QFont discFont = discItem->font();
            discFont.setBold(true);
            discItem->setFont(discFont);
        }
    }

    // Col 8: SALE PRICE
    auto* salePriceItem = m_cartTable->item(row, 8);
    salePriceItem->setText(cartItem.unitSalePrice.formatted(false));
    if (!cartItem.isDiscountable) {
        salePriceItem->setForeground(QBrush(QColor("#64748B")));
        salePriceItem->setFlags(salePriceItem->flags() & ~Qt::ItemIsEditable);
    } else {
        salePriceItem->setForeground(QBrush(QColor("#0F172A")));
        salePriceItem->setFlags(salePriceItem->flags() | Qt::ItemIsEditable);
        QFont spFont = salePriceItem->font();
        spFont.setBold(true);
        salePriceItem->setFont(spFont);
    }

    // Col 9: TOTAL
    auto* totalItem = m_cartTable->item(row, 9);
    totalItem->setText(cartItem.totalAmount.formatted());
    QFont totalFont = totalItem->font();
    totalFont.setBold(true);
    totalItem->setFont(totalFont);

    m_cartTable->blockSignals(false);
}

void SaleWindow::promptAddItem(const QString& queryOrBarcode)
{
    QuickAddItemDialog dlg(queryOrBarcode, this);
    if (dlg.exec() == QDialog::Accepted) {
        domain::Item newItem = dlg.createdItem();
        addItemToCart(newItem);
        m_searchBox->refreshCompleter();
        AppToast::showSuccess(this, QString("Added & scanned new item: %1").arg(newItem.name));
    }
}

void SaleWindow::updateTotals()
{
    core::Money grossTotal;
    core::Money totalLineDiscount;
    core::Money netTotal;
    int totalLines = 0;
    int totalUnits = 0;

    for (const auto& it : m_cart) {
        grossTotal += it.totalGross;
        totalLineDiscount += it.totalDiscount;
        netTotal += it.totalAmount;
        totalLines++;
        totalUnits += it.displayQty;
    }

    core::Money grandTotal = netTotal - m_discount;
    if (grandTotal.isNegative()) grandTotal = core::Money(0);
    core::Money totalAllDiscount = totalLineDiscount + m_discount;

    m_itemCountLabel->setText(QString("Lines: %1 | Qty: %2").arg(totalLines).arg(totalUnits));
    m_subtotalLabel->setText(QString("Subtotal: %1").arg(grossTotal.formatted()));
    m_discountLabel->setText(QString("Discount: %1").arg(totalAllDiscount.formatted()));
    m_totalLabel->setText(grandTotal.formatted());
}

void SaleWindow::handleCellChanged(int row, int col)
{
    if (row < 0 || row >= static_cast<int>(m_cart.size())) return;

    if (col == 5) {
        // QTY changed
        bool ok = false;
        int newQty = m_cartTable->item(row, 5)->text().toInt(&ok);
        if (!ok || newQty == 0) {
            updateTableRow(row);
            return;
        }

        m_cart[row].displayQty = newQty;
        m_cart[row].totalAtomicQty = newQty * m_cart[row].atomicUnitsPerQty;
        m_cart[row].totalGross = m_cart[row].unitMrp * newQty;
        m_cart[row].totalDiscount = m_cart[row].unitDiscount * newQty;
        m_cart[row].totalAmount = m_cart[row].unitSalePrice * newQty;

        updateTableRow(row);
        updateTotals();
        m_searchBox->setFocus();
    } else if (col == 7) {
        // DISC % changed
        if (!m_cart[row].isDiscountable) {
            updateTableRow(row);
            AppToast::showWarning(this, QString("🔒 '%1' is Non-Discountable (FMCG / MRP Locked).").arg(m_cart[row].itemName));
            return;
        }

        QString text = m_cartTable->item(row, 7)->text().replace("%", "").trimmed();
        bool ok = false;
        double enteredPct = text.toDouble(&ok);
        if (!ok || enteredPct < 0.0) {
            updateTableRow(row);
            return;
        }

        auto& dbMgr = database::DatabaseManager::instance();
        QSqlDatabase db = dbMgr.connection();
        QSqlQuery q(db);
        q.prepare(R"(
            SELECT i.*, COALESCE(c.is_discountable, 1) as cat_is_discountable,
                   COALESCE(c.default_disc_pct, 0.0) as cat_default_disc_pct,
                   COALESCE(c.max_discount_pct, 15.0) as cat_max_disc_pct
            FROM items i
            LEFT JOIN categories c ON i.category_id = c.id
            WHERE i.id = ?
        )");
        q.addBindValue(m_cart[row].itemId);

        domain::Item item;
        if (q.exec() && q.next()) {
            item.id = q.value("id").toInt();
            item.name = q.value("name").toString();
            item.salePrice = core::Money::fromPaisa(q.value("sale_price_paisa").toLongLong());
            item.purchaseCost = core::Money::fromPaisa(q.value("purchase_cost_paisa").toLongLong());
            int64_t tpVal = q.value("tp_paisa").toLongLong();
            item.tp = (tpVal > 0) ? core::Money::fromPaisa(tpVal) : item.purchaseCost;
            item.categoryDiscountable = (q.value("cat_is_discountable").toInt() == 1);
            item.categoryDefaultDiscountPct = q.value("cat_default_disc_pct").toDouble();
            item.categoryMaxDiscountPct = q.value("cat_max_disc_pct").toDouble();
            if (item.categoryMaxDiscountPct <= 0.0 && item.categoryDiscountable) {
                item.categoryMaxDiscountPct = 15.0;
            }
            if (!q.value("item_is_discountable").isNull()) {
                item.isDiscountableOverride = (q.value("item_is_discountable").toInt() == 1);
            }
            if (!q.value("override_disc_pct").isNull()) {
                item.discountPctOverride = q.value("override_disc_pct").toDouble();
            }
            item.minMarginPct = q.value("min_margin_pct").toDouble();
        }

        auto comp = services::PriceCalculator::calculateRowTotals(
            item,
            item.categoryDiscountable,
            item.categoryDefaultDiscountPct,
            item.categoryMaxDiscountPct,
            m_cart[row].displayQty,
            enteredPct,
            m_cart[row].unitMrp,
            m_cart[row].unitTp
        );

        m_cart[row].discountPct = comp.discountPct;
        m_cart[row].unitDiscount = comp.unitDiscount;
        m_cart[row].unitSalePrice = comp.unitSalePrice;
        m_cart[row].unitPrice = comp.unitSalePrice;
        m_cart[row].totalGross = comp.totalGross;
        m_cart[row].totalDiscount = comp.totalDiscount;
        m_cart[row].totalAmount = comp.totalAmount;

        if (comp.wasClampedDueToLoss) {
            AppToast::showWarning(this, QString("⚠ Margin Protection: Clamped to %1% (Trade Price floor %2).")
                .arg(QString::number(comp.discountPct, 'f', 1), comp.unitTp.formatted()));
        }

        updateTableRow(row);
        updateTotals();
        m_searchBox->setFocus();
    } else if (col == 8) {
        // SALE PRICE changed (Reverse calculation)
        if (!m_cart[row].isDiscountable) {
            updateTableRow(row);
            AppToast::showWarning(this, QString("🔒 '%1' is Non-Discountable (FMCG / MRP Locked).").arg(m_cart[row].itemName));
            return;
        }

        QString text = m_cartTable->item(row, 8)->text().trimmed();
        bool ok = false;
        double enteredPriceRupees = text.toDouble(&ok);
        if (!ok || enteredPriceRupees <= 0.0) {
            updateTableRow(row);
            return;
        }

        auto& dbMgr = database::DatabaseManager::instance();
        QSqlDatabase db = dbMgr.connection();
        QSqlQuery q(db);
        q.prepare(R"(
            SELECT i.*, COALESCE(c.is_discountable, 1) as cat_is_discountable,
                   COALESCE(c.default_disc_pct, 0.0) as cat_default_disc_pct,
                   COALESCE(c.max_discount_pct, 15.0) as cat_max_disc_pct
            FROM items i
            LEFT JOIN categories c ON i.category_id = c.id
            WHERE i.id = ?
        )");
        q.addBindValue(m_cart[row].itemId);

        domain::Item item;
        if (q.exec() && q.next()) {
            item.id = q.value("id").toInt();
            item.name = q.value("name").toString();
            item.salePrice = core::Money::fromPaisa(q.value("sale_price_paisa").toLongLong());
            item.purchaseCost = core::Money::fromPaisa(q.value("purchase_cost_paisa").toLongLong());
            int64_t tpVal = q.value("tp_paisa").toLongLong();
            item.tp = (tpVal > 0) ? core::Money::fromPaisa(tpVal) : item.purchaseCost;
            item.categoryDiscountable = (q.value("cat_is_discountable").toInt() == 1);
            item.categoryDefaultDiscountPct = q.value("cat_default_disc_pct").toDouble();
            item.categoryMaxDiscountPct = q.value("cat_max_disc_pct").toDouble();
            if (item.categoryMaxDiscountPct <= 0.0 && item.categoryDiscountable) {
                item.categoryMaxDiscountPct = 15.0;
            }
            if (!q.value("item_is_discountable").isNull()) {
                item.isDiscountableOverride = (q.value("item_is_discountable").toInt() == 1);
            }
            if (!q.value("override_disc_pct").isNull()) {
                item.discountPctOverride = q.value("override_disc_pct").toDouble();
            }
            item.minMarginPct = q.value("min_margin_pct").toDouble();
        }

        core::Money enteredPrice = core::Money::fromRupees(enteredPriceRupees);
        auto comp = services::PriceCalculator::calculateReverseFromNetPrice(
            item,
            item.categoryDiscountable,
            item.categoryMaxDiscountPct,
            m_cart[row].displayQty,
            enteredPrice,
            m_cart[row].unitMrp,
            m_cart[row].unitTp
        );

        m_cart[row].discountPct = comp.discountPct;
        m_cart[row].unitDiscount = comp.unitDiscount;
        m_cart[row].unitSalePrice = comp.unitSalePrice;
        m_cart[row].unitPrice = comp.unitSalePrice;
        m_cart[row].totalGross = comp.totalGross;
        m_cart[row].totalDiscount = comp.totalDiscount;
        m_cart[row].totalAmount = comp.totalAmount;

        if (comp.wasClampedDueToLoss) {
            AppToast::showWarning(this, QString("⚠ Margin Protection: Entered price Rs. %1 is below Trade Price (%2)! Clamped to cost floor.")
                .arg(QString::number(enteredPriceRupees, 'f', 2), comp.unitTp.formatted()));
        }

        updateTableRow(row);
        updateTotals();
        m_searchBox->setFocus();
    }
}

void SaleWindow::handleQuantityShortcut()
{
    int row = m_cartTable->currentRow();
    if (row < 0 || row >= static_cast<int>(m_cart.size())) {
        if (!m_cart.empty()) {
            row = static_cast<int>(m_cart.size()) - 1;
            m_cartTable->selectRow(row);
        } else {
            AppToast::showWarning(this, "Cart is empty.");
            return;
        }
    }

    // Zero-Modal Inline Editing: Focus directly into Qty cell
    m_cartTable->setCurrentCell(row, 5);
    m_cartTable->editItem(m_cartTable->item(row, 5));
}

void SaleWindow::handleDiscountShortcut()
{
    if (m_cart.empty()) {
        AppToast::showWarning(this, "Cart is empty.");
        m_searchBox->setFocus();
        return;
    }

    bool ok = false;
    double discPct = QInputDialog::getDouble(
        this,
        "Apply Global Bill Discount (F4)",
        "Enter Discount % across eligible pharma items:\n(FMCG & Baby Milk will remain locked at 0%)",
        10.0, 0.0, 100.0, 2, &ok
    );
    if (ok) {
        bool anyFmcgSkipped = false;
        bool anyClampedDueToLoss = false;
        services::PriceCalculator::applyGlobalBillDiscount(m_cart, discPct, anyFmcgSkipped, anyClampedDueToLoss);

        for (int r = 0; r < static_cast<int>(m_cart.size()); ++r) {
            updateTableRow(r);
        }
        updateTotals();

        QString msg = QString("Applied %1% discount to eligible lines.").arg(QString::number(discPct, 'f', 1));
        if (anyFmcgSkipped) {
            msg += " FMCG / Baby Milk locked at 0%.";
        }
        if (anyClampedDueToLoss) {
            msg += " Clamped items where discount breached Trade Price.";
        }
        AppToast::showInfo(this, msg);
    }
    m_searchBox->setFocus();
}

void SaleWindow::handleCustomerShortcut()
{
    auto custRes = services::LedgerService::instance().searchCustomers("");
    if (custRes.isErr() || custRes.value().empty()) return;

    QStringList names;
    for (const auto& c : custRes.value()) {
        names << QString("%1 (%2) - Baqaya: %3").arg(c.name, c.phone.isEmpty() ? "Walk-in" : c.phone, c.baqaya.formatted());
    }

    bool ok = false;
    QString chosen = QInputDialog::getItem(this, "Select Customer (F5)", "Choose customer account:", names, 0, false, &ok);
    if (ok) {
        int idx = names.indexOf(chosen);
        if (idx >= 0 && idx < static_cast<int>(custRes.value().size())) {
            m_currentCustomer = custRes.value()[idx];
            updateCustomerDisplay();
            AppToast::showInfo(this, QString("Customer selected: %1").arg(m_currentCustomer.name));
        }
    }
    m_searchBox->setFocus();
}

void SaleWindow::handleHoldShortcut()
{
    if (m_cart.empty()) {
        AppToast::showWarning(this, "No items to put on hold.");
        return;
    }

    QString holdId = services::SaleService::instance().holdCurrentBill(m_cart, m_currentCustomer.name);
    AppToast::showInfo(this, QString("Bill #%1 placed on hold (ID: %2)").arg(m_billNumberLabel->text(), holdId));
    resetSale();
}

void SaleWindow::handleHeldBillsShortcut()
{
    HoldBillsDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        auto restored = services::SaleService::instance().restoreHeldBill(dlg.selectedHoldId());
        if (restored.has_value()) {
            resetSale();
            m_currentCustomer.name = restored->customerName;
            updateCustomerDisplay();

            for (const auto& it : restored->items) {
                auto& dbMgr = database::DatabaseManager::instance();
                QSqlDatabase db = dbMgr.connection();
                QSqlQuery q(db);
                q.prepare(R"(
                    SELECT i.*, c.name as category_name,
                           COALESCE(c.is_discountable, 1) as cat_is_discountable,
                           COALESCE(c.default_disc_pct, 0.0) as cat_default_disc_pct,
                           COALESCE(c.max_discount_pct, 15.0) as cat_max_disc_pct
                    FROM items i
                    LEFT JOIN categories c ON i.category_id = c.id
                    WHERE i.id = ?
                )");
                q.addBindValue(it.itemId);
                if (q.exec() && q.next()) {
                    domain::Item dbItem;
                    dbItem.id = q.value("id").toInt();
                    dbItem.code = q.value("code").toString();
                    dbItem.name = q.value("name").toString();
                    dbItem.categoryId = q.value("category_id").toInt();
                    dbItem.categoryName = q.value("category_name").toString();
                    dbItem.brand = q.value("brand").toString();
                    dbItem.barcode = q.value("barcode").toString();
                    dbItem.salePrice = core::Money::fromPaisa(q.value("sale_price_paisa").toLongLong());
                    dbItem.purchaseCost = core::Money::fromPaisa(q.value("purchase_cost_paisa").toLongLong());
                    int64_t tpVal = q.value("tp_paisa").toLongLong();
                    dbItem.tp = (tpVal > 0) ? core::Money::fromPaisa(tpVal) : dbItem.purchaseCost;
                    dbItem.isMedicine = (q.value("is_medicine").toInt() == 1);
                    dbItem.piecesPerStrip = q.value("pieces_per_strip").toInt();
                    dbItem.stripsPerBox = q.value("strips_per_box").toInt();
                    dbItem.stripSalePrice = core::Money::fromPaisa(q.value("strip_sale_price_paisa").toLongLong());
                    dbItem.boxSalePrice = core::Money::fromPaisa(q.value("box_sale_price_paisa").toLongLong());
                    dbItem.categoryDiscountable = (q.value("cat_is_discountable").toInt() == 1);
                    dbItem.categoryDefaultDiscountPct = q.value("cat_default_disc_pct").toDouble();
                    dbItem.categoryMaxDiscountPct = q.value("cat_max_disc_pct").toDouble();
                    if (dbItem.categoryMaxDiscountPct <= 0.0 && dbItem.categoryDiscountable) {
                        dbItem.categoryMaxDiscountPct = 15.0;
                    }
                    if (!q.value("item_is_discountable").isNull()) {
                        dbItem.isDiscountableOverride = (q.value("item_is_discountable").toInt() == 1);
                    }
                    if (!q.value("override_disc_pct").isNull()) {
                        dbItem.discountPctOverride = q.value("override_disc_pct").toDouble();
                    }
                    dbItem.minMarginPct = q.value("min_margin_pct").toDouble();
                    addItemToCart(dbItem, it.displayQty, it.unitSelection);
                } else {
                    domain::Item dummyItem;
                    dummyItem.id = it.itemId;
                    dummyItem.name = it.itemName;
                    dummyItem.salePrice = it.unitMrp.isPositive() ? it.unitMrp : it.unitPrice;
                    dummyItem.tp = it.unitTp;
                    addItemToCart(dummyItem, it.displayQty, it.unitSelection);
                }
            }
            AppToast::showSuccess(this, "Held bill restored to cart.");
        }
    }
    m_searchBox->setFocus();
}

void SaleWindow::handleRemoveSelectedItem()
{
    int row = m_cartTable->currentRow();
    if (row >= 0 && row < static_cast<int>(m_cart.size())) {
        QString removedName = m_cart[row].itemName;
        m_cart.erase(m_cart.begin() + row);
        m_cartTable->blockSignals(true);
        m_cartTable->removeRow(row);
        for (int i = 0; i < m_cartTable->rowCount(); ++i) {
            m_cartTable->item(i, 0)->setText(QString::number(i + 1));
        }
        m_cartTable->blockSignals(false);
        updateTotals();
        updateEmptyState();
        AppToast::showInfo(this, QString("Removed '%1' from cart").arg(removedName));
    }
    m_searchBox->setFocus();
}

void SaleWindow::handleUnitToggleShortcut()
{
    int row = m_cartTable->currentRow();
    if (row < 0 || row >= static_cast<int>(m_cart.size())) {
        if (!m_cart.empty()) {
            row = static_cast<int>(m_cart.size()) - 1;
            m_cartTable->selectRow(row);
        } else {
            AppToast::showWarning(this, "Cart is empty.");
            return;
        }
    }

    auto& cartItem = m_cart[row];

    auto& dbMgr = database::DatabaseManager::instance();
    QSqlDatabase db = dbMgr.connection();
    if (!db.isOpen()) return;

    QSqlQuery q(db);
    q.prepare(R"(
        SELECT i.*, COALESCE(c.is_discountable, 1) as cat_is_discountable,
               COALESCE(c.default_disc_pct, 0.0) as cat_default_disc_pct,
               COALESCE(c.max_discount_pct, 15.0) as cat_max_disc_pct
        FROM items i
        LEFT JOIN categories c ON i.category_id = c.id
        WHERE i.id = ?
    )");
    q.addBindValue(cartItem.itemId);
    if (!q.exec() || !q.next()) return;

    core::Money piecePrice = core::Money::fromPaisa(q.value("sale_price_paisa").toLongLong());
    core::Money stripPrice = core::Money::fromPaisa(q.value("strip_sale_price_paisa").toLongLong());
    core::Money boxPrice = core::Money::fromPaisa(q.value("box_sale_price_paisa").toLongLong());
    core::Money purchaseCost = core::Money::fromPaisa(q.value("purchase_cost_paisa").toLongLong());
    int64_t tpVal = q.value("tp_paisa").toLongLong();
    core::Money baseTp = (tpVal > 0) ? core::Money::fromPaisa(tpVal) : purchaseCost;

    int piecesPerStrip = q.value("pieces_per_strip").toInt();
    int stripsPerBox = q.value("strips_per_box").toInt();
    bool isMedicine = (q.value("is_medicine").toInt() == 1);

    if (piecesPerStrip <= 1 && stripsPerBox <= 1) {
        AppToast::showInfo(this, QString("'%1' has single unit packaging only.").arg(cartItem.itemName));
        return;
    }

    domain::SaleUnitSelection nextUnit = domain::SaleUnitSelection::PieceOrTablet;
    QString unitStr = isMedicine ? "Tablet" : "Piece";
    int atomicPerUnit = 1;
    core::Money newUnitMrp = piecePrice;
    core::Money newUnitTp = baseTp;

    if (cartItem.unitSelection == domain::SaleUnitSelection::PieceOrTablet) {
        if (piecesPerStrip > 1) {
            nextUnit = domain::SaleUnitSelection::Strip;
            unitStr = "Strip";
            atomicPerUnit = piecesPerStrip;
            newUnitMrp = stripPrice.isPositive() ? stripPrice : (piecePrice * piecesPerStrip);
            newUnitTp = baseTp * piecesPerStrip;
        } else if (stripsPerBox > 1) {
            nextUnit = domain::SaleUnitSelection::Box;
            unitStr = "Box";
            atomicPerUnit = stripsPerBox;
            newUnitMrp = boxPrice.isPositive() ? boxPrice : (piecePrice * stripsPerBox);
            newUnitTp = baseTp * stripsPerBox;
        }
    } else if (cartItem.unitSelection == domain::SaleUnitSelection::Strip) {
        if (stripsPerBox > 1) {
            nextUnit = domain::SaleUnitSelection::Box;
            unitStr = "Box";
            atomicPerUnit = stripsPerBox * piecesPerStrip;
            newUnitMrp = boxPrice.isPositive() ? boxPrice : (piecePrice * atomicPerUnit);
            newUnitTp = baseTp * atomicPerUnit;
        } else {
            nextUnit = domain::SaleUnitSelection::PieceOrTablet;
            unitStr = isMedicine ? "Tablet" : "Piece";
            atomicPerUnit = 1;
            newUnitMrp = piecePrice;
            newUnitTp = baseTp;
        }
    } else { // Currently Box
        nextUnit = domain::SaleUnitSelection::PieceOrTablet;
        unitStr = isMedicine ? "Tablet" : "Piece";
        atomicPerUnit = 1;
        newUnitMrp = piecePrice;
        newUnitTp = baseTp;
    }

    domain::Item item;
    item.id = cartItem.itemId;
    item.name = cartItem.itemName;
    item.categoryDiscountable = (q.value("cat_is_discountable").toInt() == 1);
    item.categoryDefaultDiscountPct = q.value("cat_default_disc_pct").toDouble();
    item.categoryMaxDiscountPct = q.value("cat_max_disc_pct").toDouble();
    if (item.categoryMaxDiscountPct <= 0.0 && item.categoryDiscountable) {
        item.categoryMaxDiscountPct = 15.0;
    }
    if (!q.value("item_is_discountable").isNull()) {
        item.isDiscountableOverride = (q.value("item_is_discountable").toInt() == 1);
    }
    if (!q.value("override_disc_pct").isNull()) {
        item.discountPctOverride = q.value("override_disc_pct").toDouble();
    }
    item.minMarginPct = q.value("min_margin_pct").toDouble();

    auto comp = services::PriceCalculator::calculateRowTotals(
        item,
        item.categoryDiscountable,
        item.categoryDefaultDiscountPct,
        item.categoryMaxDiscountPct,
        cartItem.displayQty,
        cartItem.discountPct,
        newUnitMrp,
        newUnitTp
    );

    cartItem.unitSelection = nextUnit;
    cartItem.atomicUnitsPerQty = atomicPerUnit;
    cartItem.totalAtomicQty = cartItem.displayQty * atomicPerUnit;
    cartItem.unitMrp = comp.unitMrp;
    cartItem.unitTp = comp.unitTp;
    cartItem.isDiscountable = comp.isDiscountable;
    cartItem.discountPct = comp.discountPct;
    cartItem.unitDiscount = comp.unitDiscount;
    cartItem.unitSalePrice = comp.unitSalePrice;
    cartItem.unitPrice = comp.unitSalePrice;
    cartItem.totalGross = comp.totalGross;
    cartItem.totalDiscount = comp.totalDiscount;
    cartItem.totalAmount = comp.totalAmount;

    updateTableRow(row);
    updateTotals();
    AppToast::showInfo(this, QString("Switched to %1 (%2)").arg(unitStr, comp.unitSalePrice.formatted()));
}

void SaleWindow::handleIncrementQty()
{
    int row = m_cartTable->currentRow();
    if (row < 0 || row >= static_cast<int>(m_cart.size())) {
        if (!m_cart.empty()) {
            row = static_cast<int>(m_cart.size()) - 1;
            m_cartTable->selectRow(row);
        } else {
            return;
        }
    }

    m_cart[row].displayQty += 1;
    m_cart[row].totalAtomicQty = m_cart[row].displayQty * m_cart[row].atomicUnitsPerQty;
    m_cart[row].totalGross = m_cart[row].unitMrp * m_cart[row].displayQty;
    m_cart[row].totalDiscount = m_cart[row].unitDiscount * m_cart[row].displayQty;
    m_cart[row].totalAmount = m_cart[row].unitSalePrice * m_cart[row].displayQty;

    updateTableRow(row);
    updateTotals();
}

void SaleWindow::handleDecrementQty()
{
    int row = m_cartTable->currentRow();
    if (row < 0 || row >= static_cast<int>(m_cart.size())) {
        if (!m_cart.empty()) {
            row = static_cast<int>(m_cart.size()) - 1;
            m_cartTable->selectRow(row);
        } else {
            return;
        }
    }

    if (m_cart[row].displayQty > 1 || m_cart[row].displayQty < -1) {
        m_cart[row].displayQty -= (m_cart[row].displayQty > 0 ? 1 : -1);
        m_cart[row].totalAtomicQty = m_cart[row].displayQty * m_cart[row].atomicUnitsPerQty;
        m_cart[row].totalGross = m_cart[row].unitMrp * m_cart[row].displayQty;
        m_cart[row].totalDiscount = m_cart[row].unitDiscount * m_cart[row].displayQty;
        m_cart[row].totalAmount = m_cart[row].unitSalePrice * m_cart[row].displayQty;

        updateTableRow(row);
        updateTotals();
    } else {
        handleRemoveSelectedItem();
    }
}

void SaleWindow::handleChillarRoundShortcut()
{
    core::Money netTotal;
    for (const auto& it : m_cart) netTotal += it.totalAmount;
    core::Money currentNet = netTotal - m_discount;
    if (currentNet.isNegative() || currentNet.paisa() == 0) {
        AppToast::showWarning(this, "No payable amount to round.");
        return;
    }

    // Pakistani Chillar Rounding: Round down to nearest multiple of Rs. 5
    // Example: Rs. 683 -> Rs. 680 (difference of Rs. 3 added to discount)
    int64_t rupees = currentNet.paisa() / 100;
    int64_t remainder = rupees % 5;
    if (remainder > 0) {
        core::Money roundDiff = core::Money::fromRupees(remainder);
        m_discount += roundDiff;
        updateTotals();
        AppToast::showInfo(this, QString("Chillar rounded to nearest Rs. 5 (Applied %1 round-off discount)").arg(roundDiff.formatted()));
    } else {
        AppToast::showInfo(this, "Bill is already rounded to nearest Rs. 5.");
    }
}

void SaleWindow::handleMutabadilShortcut()
{
    if (m_mutabadilDrawer->isVisible()) {
        m_mutabadilDrawer->setVisible(false);
        m_searchBox->setFocus();
        return;
    }

    QString genericName;
    int currentItemId = 0;

    int row = m_cartTable->currentRow();
    if (row >= 0 && row < static_cast<int>(m_cart.size())) {
        currentItemId = m_cart[row].itemId;
        auto& dbMgr = database::DatabaseManager::instance();
        QSqlDatabase db = dbMgr.connection();
        QSqlQuery q(db);
        q.prepare("SELECT generic_name, name FROM items WHERE id = ?");
        q.addBindValue(currentItemId);
        if (q.exec() && q.next()) {
            genericName = q.value(0).toString().trimmed();
        }
    }

    if (genericName.isEmpty() && !m_searchBox->text().trimmed().isEmpty()) {
        genericName = m_searchBox->text().trimmed();
    }

    if (genericName.isEmpty()) {
        AppToast::showWarning(this, "Select an item in cart or type generic formula in search box to find Mutabadil (Alt+M).");
        return;
    }

    loadMutabadilForGeneric(genericName, currentItemId);
}

void SaleWindow::loadMutabadilForGeneric(const QString& genericName, int excludeItemId)
{
    auto& dbMgr = database::DatabaseManager::instance();
    QSqlDatabase db = dbMgr.connection();
    if (!db.isOpen()) return;

    QSqlQuery q(db);
    q.prepare(R"(
        SELECT i.id, i.name, i.brand, i.sale_price_paisa, i.generic_name,
               COALESCE(SUM(b.quantity_remaining), 0) as total_stock
        FROM items i
        LEFT JOIN batches b ON i.id = b.item_id AND b.quantity_remaining > 0
        WHERE i.generic_name LIKE ? AND i.is_active = 1
        GROUP BY i.id
        ORDER BY i.sale_price_paisa ASC
    )");
    q.addBindValue("%" + genericName + "%");

    if (!q.exec()) return;

    m_mutabadilTitleLabel->setText(QString("Formula: <b>%1</b>").arg(genericName));
    m_mutabadilTable->setRowCount(0);

    core::Money referencePrice;
    if (excludeItemId > 0) {
        for (const auto& it : m_cart) {
            if (it.itemId == excludeItemId) {
                referencePrice = it.unitSalePrice;
                break;
            }
        }
    }

    int row = 0;
    while (q.next()) {
        int id = q.value("id").toInt();
        QString name = q.value("name").toString();
        QString brand = q.value("brand").toString();
        core::Money price = core::Money::fromPaisa(q.value("sale_price_paisa").toLongLong());
        int stock = q.value("total_stock").toInt();

        m_mutabadilTable->insertRow(row);

        auto* nameItem = new QTableWidgetItem(name);
        nameItem->setData(Qt::UserRole, id);
        if (id == excludeItemId) {
            nameItem->setText(name + " (Current)");
            nameItem->setForeground(QBrush(QColor("#0F766E")));
        }

        auto* brandItem = new QTableWidgetItem(brand.isEmpty() ? "-" : brand);
        auto* priceItem = new QTableWidgetItem(price.formatted(false));
        priceItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

        QString savingsStr = "-";
        if (referencePrice.isPositive() && price < referencePrice) {
            core::Money save = referencePrice - price;
            savingsStr = QString("-%1").arg(save.formatted(false));
        }
        auto* saveItem = new QTableWidgetItem(savingsStr);
        saveItem->setTextAlignment(Qt::AlignCenter);
        if (savingsStr != "-") {
            saveItem->setForeground(QBrush(QColor("#16A34A")));
            saveItem->setFont(QFont("", -1, QFont::Bold));
        }

        auto* stockItem = new QTableWidgetItem(QString::number(stock));
        stockItem->setTextAlignment(Qt::AlignCenter);
        if (stock == 0) {
            stockItem->setForeground(QBrush(QColor("#DC2626")));
        }

        m_mutabadilTable->setItem(row, 0, nameItem);
        m_mutabadilTable->setItem(row, 1, priceItem);
        m_mutabadilTable->setItem(row, 2, saveItem);
        m_mutabadilTable->setItem(row, 3, stockItem);

        row++;
    }

    if (row == 0) {
        AppToast::showInfo(this, QString("No other brands found for '%1'").arg(genericName));
        m_mutabadilDrawer->setVisible(false);
        return;
    }

    m_mutabadilDrawer->setVisible(true);
}

void SaleWindow::handleCheckoutShortcut()
{
    if (m_cart.empty()) {
        AppToast::showWarning(this, "Please scan or add items before checkout.");
        return;
    }

    // Safety Check: DRAP Compliance for Expired Medicines
    for (const auto& it : m_cart) {
        if (it.expiryDate.isValid() && it.displayQty > 0) {
            if (QDate::currentDate().daysTo(it.expiryDate) <= 0) {
                int res = QMessageBox::critical(
                    this,
                    "EXPIRED MEDICINE ALERT",
                    QString("CRITICAL WARNING:\n'%1' (Batch: %2) is EXPIRED (%3)!\n\nDRAP regulations prohibit dispensing expired medicines.\nRemove this item before proceeding?")
                        .arg(it.itemName, it.batchNumber, it.expiryDate.toString("dd-MMM-yyyy")),
                    QMessageBox::Yes | QMessageBox::No
                );
                if (res == QMessageBox::Yes) {
                    return;
                }
            }
        }
    }

    core::Money grossTotal;
    core::Money totalLineDiscount;
    core::Money netTotal;
    for (const auto& it : m_cart) {
        grossTotal += it.totalGross;
        totalLineDiscount += it.totalDiscount;
        netTotal += it.totalAmount;
    }

    core::Money grandTotal = netTotal - m_discount;
    if (grandTotal.isNegative()) grandTotal = core::Money(0);
    core::Money totalAllDiscount = totalLineDiscount + m_discount;

    PaymentDialog dlg(grandTotal, m_currentCustomer, this);
    if (dlg.exec() == QDialog::Accepted) {
        domain::Sale sale;
        sale.customerId = m_currentCustomer.id;
        sale.customerName = m_currentCustomer.name;
        sale.paymentType = dlg.selectedPaymentType();
        sale.payments = dlg.paymentAllocations();
        sale.subtotal = grossTotal;
        sale.discount = totalAllDiscount;
        sale.netTotal = grandTotal;
        sale.cashReceived = dlg.cashReceived();
        sale.changeGiven = dlg.changeGiven();
        sale.items = m_cart;
        sale.cashierName = app::AppContext::instance().currentUser().fullName;

        int userId = app::AppContext::instance().currentUser().id;
        auto saleResult = services::SaleService::instance().completeSale(sale, userId);
        if (saleResult.isErr()) {
            QMessageBox::critical(this, "Sale Error", saleResult.error().userMessage());
            return;
        }

        domain::Sale completedSale = saleResult.value();

        QString defaultPrinter = app::Configuration::instance().settings().defaultPrinterName;
        QByteArray escposBytes = printing::ReceiptRenderer::renderEscPos(completedSale);
        
        auto printResult = printing::ESCPOSPrinter::sendRawToPrinter(defaultPrinter, escposBytes);
        if (printResult.isErr() && !defaultPrinter.isEmpty()) {
            QMessageBox msg(this);
            msg.setWindowTitle("Sale Saved Successfully");
            msg.setText(QString("Sale %1 saved successfully.\nReceipt could not be printed: %2").arg(completedSale.billNumber, printResult.error().userMessage()));
            msg.addButton("Print Again", QMessageBox::ActionRole);
            msg.addButton("Close", QMessageBox::RejectRole);
            if (msg.exec() == QMessageBox::ActionRole) {
                printing::ESCPOSPrinter::sendRawToPrinter(defaultPrinter, escposBytes);
            }
        }

        AppToast::showSuccess(this, QString("Sale %1 completed successfully!").arg(completedSale.billNumber));
        resetSale();
    }
}

void SaleWindow::keyPressEvent(QKeyEvent* event)
{
    if (event->modifiers() & Qt::AltModifier) {
        if (event->key() == Qt::Key_M) {
            handleMutabadilShortcut();
            event->accept();
            return;
        } else if (event->key() == Qt::Key_R) {
            handleChillarRoundShortcut();
            event->accept();
            return;
        }
    }

    switch (event->key()) {
    case Qt::Key_F2:
        handleQuantityShortcut();
        event->accept();
        return;
    case Qt::Key_F3:
        m_searchBox->setFocus();
        m_searchBox->selectAll();
        event->accept();
        return;
    case Qt::Key_F4:
        handleDiscountShortcut();
        event->accept();
        return;
    case Qt::Key_F5:
        handleCustomerShortcut();
        event->accept();
        return;
    case Qt::Key_F6:
        handleUnitToggleShortcut();
        event->accept();
        return;
    case Qt::Key_F7:
        handleHoldShortcut();
        event->accept();
        return;
    case Qt::Key_F8:
        handleHeldBillsShortcut();
        event->accept();
        return;
    case Qt::Key_F9:
        handleCheckoutShortcut();
        event->accept();
        return;
    case Qt::Key_Delete:
        handleRemoveSelectedItem();
        event->accept();
        return;
    case Qt::Key_Plus:
    case Qt::Key_Equal:
        if (!m_searchBox->hasFocus()) {
            handleIncrementQty();
            event->accept();
            return;
        }
        break;
    case Qt::Key_Minus:
        if (!m_searchBox->hasFocus()) {
            handleDecrementQty();
            event->accept();
            return;
        }
        break;
    case Qt::Key_U:
        if (!m_searchBox->hasFocus()) {
            handleUnitToggleShortcut();
            event->accept();
            return;
        }
        break;
    case Qt::Key_Escape:
        if (m_mutabadilDrawer && m_mutabadilDrawer->isVisible()) {
            m_mutabadilDrawer->setVisible(false);
            m_searchBox->setFocus();
            event->accept();
            return;
        }
        m_searchBox->clear();
        m_searchBox->setFocus();
        event->accept();
        return;
    default:
        break;
    }
    QWidget::keyPressEvent(event);
}

bool SaleWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_cartTable && event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Up) {
            if (m_cartTable->currentRow() <= 0) {
                m_searchBox->setFocus();
                m_searchBox->selectAll();
                keyEvent->accept();
                return true; // Seamless jump from top of cart back into search box!
            }
        } else if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            int row = m_cartTable->currentRow();
            if (row >= 0 && row < m_cartTable->rowCount()) {
                handleQuantityShortcut();
                keyEvent->accept();
                return true; // Open inline quantity editor on Enter
            }
        } else if (keyEvent->key() == Qt::Key_Escape) {
            m_searchBox->setFocus();
            m_searchBox->clear();
            keyEvent->accept();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

} // namespace ui
