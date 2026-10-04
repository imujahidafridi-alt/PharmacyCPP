#pragma once
#include <QMainWindow>
#include <QStackedWidget>
#include <QLabel>
#include <QButtonGroup>
#include <QPushButton>

namespace ui {

class SaleWindow;
class ItemsWindow;
class StockWindow;
class ExpiryWindow;
class PurchasesWindow;
class CustomersWindow;
class SuppliersWindow;
class SaleReturnDialog;
class ReportsWindow;
class SettingsWindow;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void handleNavClicked(int id);
    void updateClock();

private:
    void setupUi();

    QLabel* m_userLabel;
    QLabel* m_clockLabel;
    
    QButtonGroup* m_navGroup;
    QStackedWidget* m_stackedWidget;

    SaleWindow* m_saleWindow;
    ItemsWindow* m_itemsWindow;
    StockWindow* m_stockWindow;
    ExpiryWindow* m_expiryWindow;
    PurchasesWindow* m_purchasesWindow;
    CustomersWindow* m_customersWindow;
    SuppliersWindow* m_suppliersWindow;
    SaleReturnDialog* m_returnsWindow;
    ReportsWindow* m_reportsWindow;
    SettingsWindow* m_settingsWindow;
};

} // namespace ui
