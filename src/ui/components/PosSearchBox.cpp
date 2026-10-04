#include "ui/components/PosSearchBox.h"
#include "ui/components/DataTable.h"
#include "database/DatabaseManager.h"
#include <QApplication>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QScreen>
#include <QSqlQuery>
#include <QSqlError>
#include <QRegularExpression>
#include <QDebug>
#include <algorithm>

namespace ui {

PosSearchPopup::PosSearchPopup(QWidget* parent)
    : QFrame(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
{
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setStyleSheet(R"(
        PosSearchPopup {
            background-color: #FFFFFF;
            border: 1.5px solid #0F766E;
            border-radius: 4px;
        }
        PosSearchPopup DataTable {
            border: none;
            border-radius: 0px;
        }
    )");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(0);

    m_table = new DataTable(this);
    m_table->setupHeaders({"PRODUCT (BRAND)", "GENERIC FORMULA", "PACKING", "PRICE", "STOCK"});
    m_table->setFocusPolicy(Qt::NoFocus);
    m_table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_table->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    // Disable automatic stretching of last section so STOCK doesn't expand indefinitely
    m_table->horizontalHeader()->setStretchLastSection(false);

    // Column 0 (Product): Stretches to fill remaining space
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);

    // Column 1 (Generic Formula): Generous width (280px) so compound formulas are never truncated
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_table->setColumnWidth(1, 280);

    // Column 2 (Packing): Centered, 110px
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    m_table->setColumnWidth(2, 110);

    // Column 3 (Price): Right-aligned, 100px
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    m_table->setColumnWidth(3, 100);

    // Column 4 (Stock): Centered badge, 120px
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
    m_table->setColumnWidth(4, 120);

    // Align column header titles to match the data content alignment perfectly:
    if (auto* h0 = m_table->horizontalHeaderItem(0)) h0->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    if (auto* h1 = m_table->horizontalHeaderItem(1)) h1->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    if (auto* h2 = m_table->horizontalHeaderItem(2)) h2->setTextAlignment(Qt::AlignCenter);
    if (auto* h3 = m_table->horizontalHeaderItem(3)) h3->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    if (auto* h4 = m_table->horizontalHeaderItem(4)) h4->setTextAlignment(Qt::AlignCenter);

    m_table->verticalHeader()->setDefaultSectionSize(30);

    layout->addWidget(m_table);

    auto chooseRow = [this](int row) {
        if (row >= 0 && row < static_cast<int>(m_results.size())) {
            emit itemChosen(m_results[row].item);
        }
    };

    connect(m_table, &QTableWidget::cellClicked, this, [chooseRow](int row, int /*col*/) {
        chooseRow(row);
    });

    connect(m_table, &QTableWidget::cellDoubleClicked, this, [chooseRow](int row, int /*col*/) {
        chooseRow(row);
    });
}

void PosSearchPopup::setResults(const std::vector<SearchResultItem>& results)
{
    m_results = results;
    m_table->blockSignals(true);
    m_table->setRowCount(0);

    for (size_t i = 0; i < results.size(); ++i) {
        const auto& entry = results[i];
        const auto& it = entry.item;
        int r = m_table->rowCount();
        m_table->insertRow(r);

        // Product Name & Brand
        QString nameWithBrand = it.brand.isEmpty() ? it.name : QString("%1 (%2)").arg(it.name, it.brand);
        auto* nameItem = new QTableWidgetItem(nameWithBrand);
        nameItem->setFont(QFont("", -1, QFont::Bold));

        // Generic Formula
        auto* genItem = new QTableWidgetItem(it.genericName.isEmpty() ? "-" : it.genericName);
        genItem->setForeground(QBrush(QColor("#475569")));

        // Packaging
        QString packStr = it.dosageForm;
        if (it.piecesPerStrip > 1) {
            packStr = QString("Strip (%1s)").arg(it.piecesPerStrip);
        } else if (packStr.isEmpty()) {
            packStr = it.isMedicine ? "Tablet" : "Piece";
        }
        auto* packItem = new QTableWidgetItem(packStr);
        packItem->setTextAlignment(Qt::AlignCenter);

        // Price
        auto* priceItem = new QTableWidgetItem(it.salePrice.formatted(false));
        priceItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        priceItem->setFont(QFont("", -1, QFont::Bold));

        // Stock Badge
        auto* stockItem = new QTableWidgetItem();
        stockItem->setTextAlignment(Qt::AlignCenter);
        
        if (entry.totalStock > 0) {
            stockItem->setText(QString("%1 Avail").arg(entry.totalStock));
            stockItem->setBackground(QBrush(QColor("#DCFCE7"))); // Soft green
            stockItem->setForeground(QBrush(QColor("#15803D")));
            stockItem->setFont(QFont("", -1, QFont::Bold));
        } else {
            stockItem->setText("0 Out");
            stockItem->setBackground(QBrush(QColor("#FEE2E2"))); // Soft red
            stockItem->setForeground(QBrush(QColor("#B91C1C")));
            stockItem->setFont(QFont("", -1, QFont::Bold));
        }

        m_table->setItem(r, 0, nameItem);
        m_table->setItem(r, 1, genItem);
        m_table->setItem(r, 2, packItem);
        m_table->setItem(r, 3, priceItem);
        m_table->setItem(r, 4, stockItem);
    }

    m_table->blockSignals(false);

    if (!results.empty()) {
        m_table->selectRow(0);
    }

    int rowHeight = 30;
    int headerHeight = m_table->horizontalHeader()->height();
    if (headerHeight < 28) headerHeight = 28;
    int calculatedHeight = static_cast<int>(results.size()) * rowHeight + headerHeight + 6;
    setFixedHeight(std::min(calculatedHeight, 340));
}

bool PosSearchPopup::hasSelection() const
{
    return m_table->currentRow() >= 0 && m_table->currentRow() < static_cast<int>(m_results.size());
}

domain::Item PosSearchPopup::selectedItem() const
{
    if (hasSelection()) {
        return m_results[m_table->currentRow()].item;
    }
    return domain::Item{};
}

void PosSearchPopup::selectNextRow()
{
    int r = m_table->currentRow();
    if (r < m_table->rowCount() - 1) {
        m_table->selectRow(r + 1);
        m_table->scrollToItem(m_table->item(r + 1, 0));
    }
}

void PosSearchPopup::selectPrevRow()
{
    int r = m_table->currentRow();
    if (r > 0) {
        m_table->selectRow(r - 1);
        m_table->scrollToItem(m_table->item(r - 1, 0));
    }
}

// -------------------------------------------------------------------------
// PosSearchBox Implementation
// -------------------------------------------------------------------------

PosSearchBox::ParsedCommand PosSearchBox::parseInput(const QString& input)
{
    ParsedCommand cmd;
    QString val = input.trimmed();
    if (val.isEmpty()) return cmd;

    // 1. Check for Wapsi / Return prefix: e.g. "-Panadol" or "-2*Panadol"
    if (val.startsWith('-')) {
        cmd.isReturn = true;
        val = val.mid(1).trimmed();
    }

    // Check if user just typed e.g. "5*" (waiting for item name)
    static const QRegularExpression rxWaitingItem(R"(^(\d+)\s*\*$)");
    auto matchWait = rxWaitingItem.match(val);
    if (matchWait.hasMatch()) {
        cmd.qty = matchWait.captured(1).toInt();
        cmd.query = "";
        return cmd;
    }

    // 2. Check for Quantity prefix: e.g. "5*Panadol" or "5 * Panadol"
    static const QRegularExpression rxQty(R"(^(\d+)\s*\*\s*(.+)$)");
    auto matchQty = rxQty.match(val);
    if (matchQty.hasMatch()) {
        cmd.qty = matchQty.captured(1).toInt();
        val = matchQty.captured(2).trimmed();
    }

    // 3. Check for Unit suffix: e.g. "*S" (Strip), "*B" (Box), "*T" or "*P" (Tablet/Piece)
    static const QRegularExpression rxUnit(R"(^(.*?)\s*\*\s*([SBTsbtPp])\s*$)");
    auto matchUnit = rxUnit.match(val);
    if (matchUnit.hasMatch()) {
        val = matchUnit.captured(1).trimmed();
        QChar uChar = matchUnit.captured(2).toUpper().at(0);
        cmd.hasExplicitUnit = true;
        if (uChar == 'S') {
            cmd.unit = domain::SaleUnitSelection::Strip;
        } else if (uChar == 'B') {
            cmd.unit = domain::SaleUnitSelection::Box;
        } else {
            cmd.unit = domain::SaleUnitSelection::PieceOrTablet;
        }
    }

    cmd.query = val;

    // 4. Check if query is all digits and at least 4 chars -> treat as barcode
    bool allDigits = (!cmd.query.isEmpty());
    for (const QChar& ch : cmd.query) {
        if (!ch.isDigit()) {
            allDigits = false;
            break;
        }
    }
    cmd.isBarcode = (allDigits && cmd.query.length() >= 4);

    if (cmd.isReturn) {
        cmd.qty = -std::abs(cmd.qty);
    }

    return cmd;
}

PosSearchBox::PosSearchBox(QWidget* parent) : QLineEdit(parent)
{
    setObjectName("posSearchBox");
    setPlaceholderText("Scan barcode or type 5*Panadol / -Wapsi / Alt+M for Generic (F3)...");
    setClearButtonEnabled(true);

    if (qApp) {
        qApp->installEventFilter(this);
    }

    connect(this, &QLineEdit::textEdited, this, &PosSearchBox::handleTextEdited);
}

PosSearchBox::~PosSearchBox()
{
    if (qApp) {
        qApp->removeEventFilter(this);
    }
    if (m_popup) {
        m_popup->deleteLater();
        m_popup = nullptr;
    }
}

void PosSearchBox::ensurePopup()
{
    if (!m_popup) {
        QWidget* top = window();
        if (top == this) top = nullptr;
        m_popup = new PosSearchPopup(top);
        connect(m_popup, &PosSearchPopup::itemChosen, this, &PosSearchBox::handlePopupItemChosen);
    }
}

void PosSearchBox::refreshCompleter()
{
    // Live multi-column queries query the database on demand, ensuring zero RAM bloat.
}

void PosSearchBox::updatePopupPosition()
{
    if (!m_popup) return;
    
    QPoint globalPos = mapToGlobal(QPoint(0, height() + 1));
    int popupWidth = std::max(width(), 780);
    int popupHeight = m_popup->height();

    // Check screen bounds to prevent clipping off the bottom
    QScreen* screen = window()->screen();
    if (screen) {
        QRect screenGeo = screen->availableGeometry();
        if (globalPos.y() + popupHeight > screenGeo.bottom()) {
            globalPos.setY(mapToGlobal(QPoint(0, 0)).y() - popupHeight - 2);
        }
    }

    m_popup->setGeometry(globalPos.x(), globalPos.y(), popupWidth, popupHeight);
}

void PosSearchBox::handleTextEdited(const QString& text)
{
    QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        if (m_popup) m_popup->hide();
        return;
    }

    ParsedCommand cmd = parseInput(trimmed);
    if (cmd.isBarcode || cmd.query.isEmpty()) {
        if (m_popup) m_popup->hide();
        return;
    }

    auto& dbMgr = database::DatabaseManager::instance();
    QSqlDatabase db = dbMgr.connection();
    if (!db.isOpen()) {
        if (m_popup) m_popup->hide();
        return;
    }

    QSqlQuery q(db);
    q.prepare(R"(
        SELECT i.id, i.code, i.name, i.category_id, c.name as category_name, i.brand, i.barcode,
               i.sale_price_paisa, i.purchase_cost_paisa, i.tp_paisa, i.min_stock_alert, i.is_active,
               i.is_medicine, i.generic_name, i.strength, i.dosage_form,
               i.pieces_per_strip, i.strips_per_box, i.strip_sale_price_paisa, i.box_sale_price_paisa,
               i.is_discountable as item_is_discountable, i.override_disc_pct, i.min_margin_pct,
               COALESCE(c.is_discountable, 1) as cat_is_discountable,
               COALESCE(c.default_disc_pct, 0.0) as cat_default_disc_pct,
               COALESCE(c.max_discount_pct, 15.0) as cat_max_disc_pct,
               COALESCE((SELECT SUM(sb.quantity) FROM stock_balances sb WHERE sb.item_id = i.id), 0) as total_stock
        FROM items i
        LEFT JOIN categories c ON i.category_id = c.id
        WHERE (i.name LIKE ? OR i.generic_name LIKE ? OR i.brand LIKE ? OR i.code LIKE ? OR i.barcode = ?
               OR EXISTS (SELECT 1 FROM item_barcodes ib WHERE ib.item_id = i.id AND ib.barcode = ?))
          AND i.is_active = 1
        ORDER BY (CASE WHEN (SELECT SUM(sb2.quantity) FROM stock_balances sb2 WHERE sb2.item_id = i.id) > 0 THEN 1 ELSE 0 END) DESC, i.name ASC
        LIMIT 10
    )");

    QString wildcard = "%" + cmd.query + "%";
    q.addBindValue(wildcard);
    q.addBindValue(wildcard);
    q.addBindValue(wildcard);
    q.addBindValue(wildcard);
    q.addBindValue(cmd.query);
    q.addBindValue(cmd.query);

    std::vector<SearchResultItem> results;
    if (q.exec()) {
        while (q.next()) {
            SearchResultItem res;
            res.item.id = q.value("id").toInt();
            res.item.code = q.value("code").toString();
            res.item.name = q.value("name").toString();
            res.item.categoryId = q.value("category_id").toInt();
            res.item.categoryName = q.value("category_name").toString();
            res.item.brand = q.value("brand").toString();
            res.item.barcode = q.value("barcode").toString();
            res.item.salePrice = core::Money::fromPaisa(q.value("sale_price_paisa").toLongLong());
            res.item.purchaseCost = core::Money::fromPaisa(q.value("purchase_cost_paisa").toLongLong());
            
            int64_t tpVal = q.value("tp_paisa").toLongLong();
            res.item.tp = (tpVal > 0) ? core::Money::fromPaisa(tpVal) : res.item.purchaseCost;

            res.item.isMedicine = (q.value("is_medicine").toInt() == 1);
            res.item.genericName = q.value("generic_name").toString();
            res.item.strength = q.value("strength").toString();
            res.item.dosageForm = q.value("dosage_form").toString();
            res.item.piecesPerStrip = q.value("pieces_per_strip").toInt();
            res.item.stripsPerBox = q.value("strips_per_box").toInt();
            res.item.stripSalePrice = core::Money::fromPaisa(q.value("strip_sale_price_paisa").toLongLong());
            res.item.boxSalePrice = core::Money::fromPaisa(q.value("box_sale_price_paisa").toLongLong());

            // Category & Item Discount Policies
            res.item.categoryDiscountable = (q.value("cat_is_discountable").toInt() == 1);
            res.item.categoryDefaultDiscountPct = q.value("cat_default_disc_pct").toDouble();
            res.item.categoryMaxDiscountPct = q.value("cat_max_disc_pct").toDouble();
            if (res.item.categoryMaxDiscountPct <= 0.0 && res.item.categoryDiscountable) {
                res.item.categoryMaxDiscountPct = 15.0;
            }

            if (!q.value("item_is_discountable").isNull()) {
                res.item.isDiscountableOverride = (q.value("item_is_discountable").toInt() == 1);
            }
            if (!q.value("override_disc_pct").isNull()) {
                res.item.discountPctOverride = q.value("override_disc_pct").toDouble();
            }
            res.item.minMarginPct = q.value("min_margin_pct").toDouble();

            res.totalStock = q.value("total_stock").toInt();
            results.push_back(res);
        }
    } else {
        qWarning() << "PosSearchBox query error:" << q.lastError().text();
    }

    if (!results.empty()) {
        ensurePopup();
        m_popup->setResults(results);
        updatePopupPosition();
        m_popup->show();
        m_popup->raise();
    } else {
        if (m_popup) m_popup->hide();
    }
}

void PosSearchBox::handlePopupItemChosen(const domain::Item& item)
{
    ParsedCommand cmd = parseInput(text());
    if (m_popup) m_popup->hide();
    clear();
    setFocus();

    emit itemSelected(item, cmd.qty, cmd.hasExplicitUnit ? cmd.unit : domain::SaleUnitSelection::PieceOrTablet);
}

void PosSearchBox::commitSearchCommand()
{
    if (m_isProcessingCommand) return;
    m_isProcessingCommand = true;

    QString raw = text().trimmed();
    ParsedCommand cmd = parseInput(raw);

    if (m_popup) m_popup->hide();
    clear();
    setFocus();

    if (!cmd.query.isEmpty()) {
        emit commandEntered(cmd);
    }

    m_isProcessingCommand = false;
}

void PosSearchBox::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (m_popup && m_popup->isVisible() && m_popup->hasSelection()) {
            handlePopupItemChosen(m_popup->selectedItem());
            event->accept();
            return;
        }

        commitSearchCommand();
        event->accept();
        return;
    } else if (event->key() == Qt::Key_Down) {
        if (m_popup && m_popup->isVisible()) {
            m_popup->selectNextRow();
            event->accept();
            return;
        } else {
            emit navigateToCartRequested();
            event->accept();
            return;
        }
    } else if (event->key() == Qt::Key_Up) {
        if (m_popup && m_popup->isVisible()) {
            m_popup->selectPrevRow();
            event->accept();
            return;
        }
    } else if (event->key() == Qt::Key_Escape) {
        if (m_popup && m_popup->isVisible()) {
            m_popup->hide();
            event->accept();
            return;
        }
    }

    QLineEdit::keyPressEvent(event);
}

bool PosSearchBox::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        if (m_popup && m_popup->isVisible()) {
            auto* me = static_cast<QMouseEvent*>(event);
            QPoint clickPos = me->globalPosition().toPoint();
            QRect boxRect(mapToGlobal(QPoint(0, 0)), size());
            QRect popRect(m_popup->mapToGlobal(QPoint(0, 0)), m_popup->size());
            if (!boxRect.contains(clickPos) && !popRect.contains(clickPos)) {
                m_popup->hide();
            }
        }
    }
    return QLineEdit::eventFilter(obj, event);
}

void PosSearchBox::focusOutEvent(QFocusEvent* event)
{
    if (m_popup && m_popup->isVisible()) {
        QWidget* focused = QApplication::focusWidget();
        if (focused && (focused == m_popup || m_popup->isAncestorOf(focused))) {
            QLineEdit::focusOutEvent(event);
            return;
        }
        m_popup->hide();
    }
    QLineEdit::focusOutEvent(event);
}

void PosSearchBox::moveEvent(QMoveEvent* event)
{
    QLineEdit::moveEvent(event);
    if (m_popup && m_popup->isVisible()) {
        updatePopupPosition();
    }
}

void PosSearchBox::resizeEvent(QResizeEvent* event)
{
    QLineEdit::resizeEvent(event);
    if (m_popup && m_popup->isVisible()) {
        updatePopupPosition();
    }
}

} // namespace ui
