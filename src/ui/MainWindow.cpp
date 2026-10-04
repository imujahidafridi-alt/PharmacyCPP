#include "ui/MainWindow.h"
#include "ui/sale/SaleWindow.h"
#include "ui/items/ItemsWindow.h"
#include "ui/stock/StockWindow.h"
#include "ui/expiry/ExpiryWindow.h"
#include "ui/purchases/PurchasesWindow.h"
#include "ui/customers/CustomersWindow.h"
#include "ui/suppliers/SuppliersWindow.h"
#include "ui/returns/SaleReturnDialog.h"
#include "ui/reports/ReportsWindow.h"
#include "ui/settings/SettingsWindow.h"
#include "app/AppContext.h"
#include "app/Configuration.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>
#include <QDateTime>

namespace ui {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    setupUi();

    auto* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::updateClock);
    timer->start(1000);
    updateClock();
}

void MainWindow::setupUi()
{
    setWindowTitle("Pak Pharmacy & Retail POS — Counter Terminal");
    resize(1080, 680);
    setMinimumSize(800, 520); // Small screen friendly

    auto* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    auto* rootLayout = new QVBoxLayout(centralWidget);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // 1. Compact Flat Navigation Bar (Header Row Removed per Request)
    auto* navBar = new QWidget(centralWidget);
    navBar->setObjectName("navBar");
    auto* navLayout = new QHBoxLayout(navBar);
    navLayout->setContentsMargins(8, 0, 8, 0);
    navLayout->setSpacing(2);

    m_navGroup = new QButtonGroup(this);
    m_navGroup->setExclusive(true);

    auto addNavButton = [this, navLayout](const QString& title, int id, bool checked = false) {
        auto* btn = new QPushButton(title, this);
        btn->setProperty("class", "navBtn");
        btn->setCheckable(true);
        btn->setChecked(checked);
        m_navGroup->addButton(btn, id);
        navLayout->addWidget(btn);
    };

    addNavButton("Sale (POS)", 0, true);
    addNavButton("Items", 1);
    addNavButton("Stock", 2);
    addNavButton("Purchases", 3);
    addNavButton("Expiry", 4);
    addNavButton("Customers", 5);
    addNavButton("Suppliers", 6);
    addNavButton("Returns", 7);
    addNavButton("Reports", 8);
    addNavButton("Settings", 9);

    navLayout->addStretch();

    // Small status badges on the right of navigation bar
    const auto& cfg = app::Configuration::instance().settings();
    auto* counterBadge = new QLabel(QString("C%1").arg(cfg.activeCounterId, 2, 10, QChar('0')), navBar);
    counterBadge->setObjectName("navMetaBadge");
    counterBadge->setToolTip("Counter Station ID");

    m_userLabel = new QLabel(app::AppContext::instance().currentUser().username, navBar);
    m_userLabel->setObjectName("navMetaBadge");
    m_userLabel->setToolTip("Logged-in Cashier");

    m_clockLabel = new QLabel(navBar);
    m_clockLabel->setObjectName("navMetaBadge");

    navLayout->addWidget(counterBadge);
    navLayout->addWidget(m_userLabel);
    navLayout->addWidget(m_clockLabel);

    rootLayout->addWidget(navBar);

    // 2. Stacked Content Pages
    m_stackedWidget = new QStackedWidget(centralWidget);

    m_saleWindow = new SaleWindow(this);
    m_itemsWindow = new ItemsWindow(this);
    m_stockWindow = new StockWindow(this);
    m_purchasesWindow = new PurchasesWindow(this);
    m_expiryWindow = new ExpiryWindow(this);
    m_customersWindow = new CustomersWindow(this);
    m_suppliersWindow = new SuppliersWindow(this);
    m_returnsWindow = new SaleReturnDialog(this);
    m_reportsWindow = new ReportsWindow(this);
    m_settingsWindow = new SettingsWindow(this);

    m_stackedWidget->addWidget(m_saleWindow);       // 0
    m_stackedWidget->addWidget(m_itemsWindow);      // 1
    m_stackedWidget->addWidget(m_stockWindow);      // 2
    m_stackedWidget->addWidget(m_purchasesWindow);  // 3
    m_stackedWidget->addWidget(m_expiryWindow);     // 4
    m_stackedWidget->addWidget(m_customersWindow);  // 5
    m_stackedWidget->addWidget(m_suppliersWindow);  // 6
    m_stackedWidget->addWidget(m_returnsWindow);    // 7
    m_stackedWidget->addWidget(m_reportsWindow);    // 8
    m_stackedWidget->addWidget(m_settingsWindow);   // 9

    rootLayout->addWidget(m_stackedWidget);

    connect(m_navGroup, &QButtonGroup::idClicked, this, &MainWindow::handleNavClicked);
}

void MainWindow::handleNavClicked(int id)
{
    m_stackedWidget->setCurrentIndex(id);

    // Auto-refresh modules when activated
    switch (id) {
    case 0: m_saleWindow->resetSale(); break;
    case 1: m_itemsWindow->refreshItems(); break;
    case 2: m_stockWindow->refreshStock(); break;
    case 3: m_purchasesWindow->refreshPurchases(); break;
    case 4: m_expiryWindow->refreshExpiryList(); break;
    case 5: m_customersWindow->refreshCustomers(); break;
    case 6: m_suppliersWindow->refreshSuppliers(); break;
    case 8: m_reportsWindow->refreshReports(); break;
    case 9: m_settingsWindow->loadSettings(); break;
    default: break;
    }
}

void MainWindow::updateClock()
{
    m_clockLabel->setText(QDateTime::currentDateTime().toString("hh:mm:ss AP"));
}

} // namespace ui
