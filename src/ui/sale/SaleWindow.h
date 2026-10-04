#pragma once
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <vector>
#include "domain/Models.h"
#include "ui/components/PosSearchBox.h"
#include "ui/components/DataTable.h"

namespace ui {

class SaleWindow : public QWidget {
    Q_OBJECT
public:
    explicit SaleWindow(QWidget* parent = nullptr);

    void resetSale();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void handleItemSelected(const domain::Item& item, int qty, domain::SaleUnitSelection unit);
    void handleCommandEntered(const ui::PosSearchBox::ParsedCommand& cmd);
    void handleBarcodeEntered(const QString& barcode);
    void handleSearchEntered(const QString& text);
    void handleQuantityShortcut();
    void handleDiscountShortcut();
    void handleCustomerShortcut();
    void handleHoldShortcut();
    void handleHeldBillsShortcut();
    void handleCheckoutShortcut();
    void handleRemoveSelectedItem();
    void handleUnitToggleShortcut();
    void handleIncrementQty();
    void handleDecrementQty();
    void handleChillarRoundShortcut();
    void handleMutabadilShortcut();
    void handleCellChanged(int row, int col);
    void updateTotals();

private:
    void addItemToCart(const domain::Item& item, int qty = 1, domain::SaleUnitSelection unit = domain::SaleUnitSelection::PieceOrTablet);
    void promptAddItem(const QString& queryOrBarcode);
    void updateCustomerDisplay();
    void updateEmptyState();
    void loadMutabadilForGeneric(const QString& genericName, int excludeItemId = 0);

    domain::Customer m_currentCustomer;
    std::vector<domain::CartItem> m_cart;
    core::Money m_discount;

    QLabel* m_billNumberLabel;
    QFrame* m_customerCard;
    QLabel* m_customerNameLabel;
    QLabel* m_customerBaqayaBadge;

    PosSearchBox* m_searchBox;
    DataTable* m_cartTable;
    QWidget* m_emptyStateOverlay;
    
    // Mutabadil Generic Drawer
    QFrame* m_mutabadilDrawer{nullptr};
    QLabel* m_mutabadilTitleLabel{nullptr};
    DataTable* m_mutabadilTable{nullptr};

    QLabel* m_itemCountLabel;
    QLabel* m_subtotalLabel;
    QLabel* m_discountLabel;
    QLabel* m_totalLabel;

    QPushButton* m_heldBillsBtn;
    QPushButton* m_mutabadilBtn;
};

} // namespace ui
