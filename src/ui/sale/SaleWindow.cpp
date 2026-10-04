#include "ui/sale/SaleWindow.h"
#include "ui/sale/PaymentDialog.h"
#include "ui/sale/HoldBillsDialog.h"
#include "ui/sale/CartQtyDelegate.h"
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
    m_cartTable->setupHeaders({"#", "ITEM NAME", "UNIT", "QTY (F2)", "PRICE", "TOTAL", "BATCH", "EXPIRY"});
    m_cartTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_cartTable->setColumnWidth(0, 36);
    m_cartTable->setColumnWidth(2, 70);
    m_cartTable->setColumnWidth(3, 75); // Qty inline editor column
    m_cartTable->setColumnWidth(4, 85);
    m_cartTable->setColumnWidth(5, 95);
    m_cartTable->setColumnWidth(6, 85);
    m_cartTable->setColumnWidth(7, 100);

    // Install custom inline editor delegate on Qty column (zero modal popups!)
    auto* qtyDelegate = new CartQtyDelegate(this);
    m_cartTable->setItemDelegateForColumn(3, qtyDelegate);
    m_cartTable->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed | QAbstractItemView::SelectedClicked);
    m_cartTable->installEventFilter(this);
    connect(qtyDelegate, &QAbstractItemDelegate::closeEditor, this, [this]() {
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
        if (col == 2) {
            handleUnitToggleShortcut();
        } else if (col == 3) {
            handleQuantityShortcut();
        }
    });

    connect(m_mutabadilTable, &QTableWidget::cellDoubleClicked, this, [this](int row, int /*col*/) {
        if (row < 0 || row >= m_mutabadilTable->rowCount()) return;
        int subItemId = m_mutabadilTable->item(row, 0)->data(Qt::UserRole).toInt();
        if (subItemId > 0) {
            auto& dbMgr = database::DatabaseManager::instance();
            QSqlDatabase db = dbMgr.connection();
            QSqlQuery q(db);
            q.prepare("SELECT * FROM items WHERE id = ?");
            q.addBindValue(subItemId);
            if (q.exec() && q.next()) {
                domain::Item subItem;
                subItem.id = q.value("id").toInt();
                subItem.code = q.value("code").toString();
                subItem.name = q.value("name").toString();
                subItem.brand = q.value("brand").toString();
                subItem.salePrice = core::Money::fromPaisa(q.value("sale_price_paisa").toLongLong());
                subItem.isMedicine = (q.value("is_medicine").toInt() == 1);
                subItem.piecesPerStrip = q.value("pieces_per_strip").toInt();
                subItem.stripsPerBox = q.value("strips_per_box").toInt();
                subItem.stripSalePrice = core::Money::fromPaisa(q.value("strip_sale_price_paisa").toLongLong());
                subItem.boxSalePrice = core::Money::fromPaisa(q.value("box_sale_price_paisa").toLongLong());

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
            SELECT i.*, c.name as category_name
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
            SELECT i.*, c.name as category_name
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
                SELECT i.*, c.name as category_name
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
        item.categoryName = q.value("category_name").toString();
        item.brand = q.value("brand").toString();
        item.genericName = q.value("generic_name").toString();
        item.salePrice = core::Money::fromPaisa(q.value("sale_price_paisa").toLongLong());
        item.isMedicine = (q.value("is_medicine").toInt() == 1);
        item.piecesPerStrip = q.value("pieces_per_strip").toInt();
        item.stripsPerBox = q.value("strips_per_box").toInt();
        item.stripSalePrice = core::Money::fromPaisa(q.value("strip_sale_price_paisa").toLongLong());
        item.boxSalePrice = core::Money::fromPaisa(q.value("box_sale_price_paisa").toLongLong());

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
    // Don't merge returns into regular positive lines
    for (size_t i = 0; i < m_cart.size(); ++i) {
        if (m_cart[i].itemId == item.id && m_cart[i].unitSelection == unit && ((m_cart[i].displayQty > 0 && qty > 0) || (m_cart[i].displayQty < 0 && qty < 0))) {
            m_cart[i].displayQty += qty;
            m_cart[i].totalAtomicQty = m_cart[i].displayQty * m_cart[i].atomicUnitsPerQty;
            m_cart[i].totalAmount = m_cart[i].unitPrice * m_cart[i].displayQty;
            
            m_cartTable->blockSignals(true);
            m_cartTable->item(i, 3)->setText(QString::number(m_cart[i].displayQty));
            m_cartTable->item(i, 5)->setText(m_cart[i].totalAmount.formatted());
            m_cartTable->blockSignals(false);

            updateTotals();
            return;
        }
    }

    core::Money uPrice = item.salePrice;
    int atomicPerUnit = 1;
    QString unitStr = item.isMedicine ? "Tablet" : "Piece";

    if (unit == domain::SaleUnitSelection::Box && item.stripsPerBox > 1) {
        uPrice = item.boxSalePrice.isPositive() ? item.boxSalePrice : (item.salePrice * (item.stripsPerBox * item.piecesPerStrip));
        atomicPerUnit = item.stripsPerBox * item.piecesPerStrip;
        unitStr = "Box";
    } else if (unit == domain::SaleUnitSelection::Strip && item.piecesPerStrip > 1) {
        uPrice = item.stripSalePrice.isPositive() ? item.stripSalePrice : (item.salePrice * item.piecesPerStrip);
        atomicPerUnit = item.piecesPerStrip;
        unitStr = "Strip";
    }

    auto fefoRes = services::StockService::instance().allocateFefo(item.id, std::abs(qty) * atomicPerUnit);
    domain::CartItem cartItem;
    cartItem.itemId = item.id;
    cartItem.itemName = (qty < 0) ? (item.name + " [WAPSI / RETURN]") : item.name;
    cartItem.unitSelection = unit;
    cartItem.displayQty = qty;
    cartItem.atomicUnitsPerQty = atomicPerUnit;
    cartItem.totalAtomicQty = qty * atomicPerUnit;
    cartItem.unitPrice = uPrice;
    cartItem.totalAmount = uPrice * qty;

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

    m_cartTable->setItem(r, 0, makeItem(QString::number(r + 1), Qt::AlignCenter));
    m_cartTable->setItem(r, 1, makeItem(cartItem.itemName));
    m_cartTable->setItem(r, 2, makeItem(unitStr, Qt::AlignCenter));
    m_cartTable->setItem(r, 3, makeItem(QString::number(qty), Qt::AlignCenter, true)); // Column 3: QTY inline editable
    m_cartTable->setItem(r, 4, makeItem(uPrice.formatted(false), Qt::AlignRight));
    m_cartTable->setItem(r, 5, makeItem(cartItem.totalAmount.formatted(), Qt::AlignRight));
    m_cartTable->setItem(r, 6, makeItem(cartItem.batchNumber, Qt::AlignCenter));
    
    // FEFO Expiry Alert Visual Formatting
    QString expiryStr = cartItem.expiryDate.isValid() ? cartItem.expiryDate.toString("MM/yyyy") : "-";
    auto* expItem = makeItem(expiryStr, Qt::AlignCenter);
    
    if (cartItem.expiryDate.isValid()) {
        int daysLeft = QDate::currentDate().daysTo(cartItem.expiryDate);
        if (daysLeft <= 0) {
            expItem->setText(expiryStr + " ⚠ EXPIRED");
            expItem->setBackground(QBrush(QColor("#FEE2E2")));
            expItem->setForeground(QBrush(QColor("#DC2626")));
            m_cartTable->item(r, 6)->setBackground(QBrush(QColor("#FEE2E2")));
            m_cartTable->item(r, 6)->setForeground(QBrush(QColor("#DC2626")));
            AppToast::showWarning(this, QString("⚠ WARNING: %1 is EXPIRED (Batch %2)!").arg(cartItem.itemName, cartItem.batchNumber));
        } else if (daysLeft <= 90) {
            expItem->setText(QString("%1 (%2d left)").arg(expiryStr).arg(daysLeft));
            expItem->setBackground(QBrush(QColor("#FEF3C7")));
            expItem->setForeground(QBrush(QColor("#B45309")));
            m_cartTable->item(r, 6)->setBackground(QBrush(QColor("#FEF3C7")));
            m_cartTable->item(r, 6)->setForeground(QBrush(QColor("#B45309")));
        }
    }

    if (qty < 0) {
        // Wapsi Row styling
        for (int c = 0; c < 8; ++c) {
            if (c != 3) { // keep qty editable
                auto* it = m_cartTable->item(r, c);
                if (it && c != 7) it->setBackground(QBrush(QColor("#EFF6FF")));
            }
        }
        m_cartTable->item(r, 1)->setForeground(QBrush(QColor("#1D4ED8")));
        m_cartTable->item(r, 5)->setForeground(QBrush(QColor("#DC2626")));
    }

    m_cartTable->setItem(r, 7, expItem);
    m_cartTable->blockSignals(false);

    m_cartTable->selectRow(r);
    updateTotals();
    updateEmptyState();
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
    core::Money subtotal;
    int totalLines = 0;
    int totalUnits = 0;

    for (const auto& it : m_cart) {
        subtotal += it.totalAmount;
        totalLines++;
        totalUnits += it.displayQty;
    }

    core::Money grandTotal = subtotal - m_discount;
    if (grandTotal.isNegative()) grandTotal = core::Money(0);

    m_itemCountLabel->setText(QString("Lines: %1 | Qty: %2").arg(totalLines).arg(totalUnits));
    m_subtotalLabel->setText(QString("Subtotal: %1").arg(subtotal.formatted()));
    m_discountLabel->setText(QString("Discount: %1").arg(m_discount.formatted()));
    m_totalLabel->setText(grandTotal.formatted());
}

void SaleWindow::handleCellChanged(int row, int col)
{
    if (col != 3 || row < 0 || row >= static_cast<int>(m_cart.size())) return;

    bool ok = false;
    int newQty = m_cartTable->item(row, 3)->text().toInt(&ok);
    if (ok && newQty != 0) {
        m_cart[row].displayQty = newQty;
        m_cart[row].totalAtomicQty = newQty * m_cart[row].atomicUnitsPerQty;
        m_cart[row].totalAmount = m_cart[row].unitPrice * newQty;

        m_cartTable->blockSignals(true);
        m_cartTable->item(row, 5)->setText(m_cart[row].totalAmount.formatted());
        m_cartTable->blockSignals(false);

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
    m_cartTable->setCurrentCell(row, 3);
    m_cartTable->editItem(m_cartTable->item(row, 3));
}

void SaleWindow::handleDiscountShortcut()
{
    bool ok = false;
    double discRupees = QInputDialog::getDouble(this, "Apply Bill Discount (F4)",
                                                "Discount Amount (Rs.):",
                                                m_discount.toRupees(), 0.0, 999999.0, 2, &ok);
    if (ok) {
        m_discount = core::Money::fromRupees(discRupees);
        updateTotals();
        AppToast::showInfo(this, QString("Bill discount applied: %1").arg(m_discount.formatted()));
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
                domain::Item dummyItem;
                dummyItem.id = it.itemId;
                dummyItem.name = it.itemName;
                dummyItem.salePrice = it.unitPrice;
                addItemToCart(dummyItem, it.displayQty, it.unitSelection);
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
    q.prepare("SELECT sale_price_paisa, strip_sale_price_paisa, box_sale_price_paisa, pieces_per_strip, strips_per_box, is_medicine FROM items WHERE id = ?");
    q.addBindValue(cartItem.itemId);
    if (!q.exec() || !q.next()) return;

    core::Money piecePrice = core::Money::fromPaisa(q.value("sale_price_paisa").toLongLong());
    core::Money stripPrice = core::Money::fromPaisa(q.value("strip_sale_price_paisa").toLongLong());
    core::Money boxPrice = core::Money::fromPaisa(q.value("box_sale_price_paisa").toLongLong());
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
    core::Money newUnitPrice = piecePrice;

    if (cartItem.unitSelection == domain::SaleUnitSelection::PieceOrTablet) {
        if (piecesPerStrip > 1) {
            nextUnit = domain::SaleUnitSelection::Strip;
            unitStr = "Strip";
            atomicPerUnit = piecesPerStrip;
            newUnitPrice = stripPrice.isPositive() ? stripPrice : (piecePrice * piecesPerStrip);
        } else if (stripsPerBox > 1) {
            nextUnit = domain::SaleUnitSelection::Box;
            unitStr = "Box";
            atomicPerUnit = stripsPerBox;
            newUnitPrice = boxPrice.isPositive() ? boxPrice : (piecePrice * stripsPerBox);
        }
    } else if (cartItem.unitSelection == domain::SaleUnitSelection::Strip) {
        if (stripsPerBox > 1) {
            nextUnit = domain::SaleUnitSelection::Box;
            unitStr = "Box";
            atomicPerUnit = stripsPerBox * piecesPerStrip;
            newUnitPrice = boxPrice.isPositive() ? boxPrice : (piecePrice * atomicPerUnit);
        } else {
            nextUnit = domain::SaleUnitSelection::PieceOrTablet;
            unitStr = isMedicine ? "Tablet" : "Piece";
            atomicPerUnit = 1;
            newUnitPrice = piecePrice;
        }
    } else { // Currently Box
        nextUnit = domain::SaleUnitSelection::PieceOrTablet;
        unitStr = isMedicine ? "Tablet" : "Piece";
        atomicPerUnit = 1;
        newUnitPrice = piecePrice;
    }

    cartItem.unitSelection = nextUnit;
    cartItem.atomicUnitsPerQty = atomicPerUnit;
    cartItem.totalAtomicQty = cartItem.displayQty * atomicPerUnit;
    cartItem.unitPrice = newUnitPrice;
    cartItem.totalAmount = newUnitPrice * cartItem.displayQty;

    m_cartTable->blockSignals(true);
    m_cartTable->item(row, 2)->setText(unitStr);
    m_cartTable->item(row, 4)->setText(newUnitPrice.formatted(false));
    m_cartTable->item(row, 5)->setText(cartItem.totalAmount.formatted());
    m_cartTable->blockSignals(false);

    updateTotals();
    AppToast::showInfo(this, QString("Switched to %1 (%2)").arg(unitStr, newUnitPrice.formatted()));
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
    m_cart[row].totalAmount = m_cart[row].unitPrice * m_cart[row].displayQty;

    m_cartTable->blockSignals(true);
    m_cartTable->item(row, 3)->setText(QString::number(m_cart[row].displayQty));
    m_cartTable->item(row, 5)->setText(m_cart[row].totalAmount.formatted());
    m_cartTable->blockSignals(false);
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
        m_cart[row].totalAmount = m_cart[row].unitPrice * m_cart[row].displayQty;

        m_cartTable->blockSignals(true);
        m_cartTable->item(row, 3)->setText(QString::number(m_cart[row].displayQty));
        m_cartTable->item(row, 5)->setText(m_cart[row].totalAmount.formatted());
        m_cartTable->blockSignals(false);
        updateTotals();
    } else {
        handleRemoveSelectedItem();
    }
}

void SaleWindow::handleChillarRoundShortcut()
{
    core::Money subtotal;
    for (const auto& it : m_cart) subtotal += it.totalAmount;
    core::Money currentNet = subtotal - m_discount;
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
                referencePrice = it.unitPrice;
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

    core::Money subtotal;
    for (const auto& it : m_cart) subtotal += it.totalAmount;
    core::Money grandTotal = subtotal - m_discount;
    if (grandTotal.isNegative()) grandTotal = core::Money(0);

    PaymentDialog dlg(grandTotal, m_currentCustomer, this);
    if (dlg.exec() == QDialog::Accepted) {
        domain::Sale sale;
        sale.customerId = m_currentCustomer.id;
        sale.customerName = m_currentCustomer.name;
        sale.paymentType = dlg.selectedPaymentType();
        sale.payments = dlg.paymentAllocations();
        sale.subtotal = subtotal;
        sale.discount = m_discount;
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
