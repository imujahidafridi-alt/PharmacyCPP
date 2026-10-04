#pragma once
#include <QLineEdit>
#include <QFrame>
#include <vector>
#include "domain/Models.h"

namespace ui {

class DataTable;

struct SearchResultItem {
    domain::Item item;
    int totalStock{0};
};

class PosSearchPopup : public QFrame {
    Q_OBJECT
public:
    explicit PosSearchPopup(QWidget* parent = nullptr);

    void setResults(const std::vector<SearchResultItem>& results);
    bool hasSelection() const;
    domain::Item selectedItem() const;

    void selectNextRow();
    void selectPrevRow();

signals:
    void itemChosen(const domain::Item& item);

private:
    DataTable* m_table{nullptr};
    std::vector<SearchResultItem> m_results;
};

class PosSearchBox : public QLineEdit {
    Q_OBJECT
public:
    explicit PosSearchBox(QWidget* parent = nullptr);
    ~PosSearchBox() override;

    void refreshCompleter();

    struct ParsedCommand {
        QString query;
        int qty{1};
        bool isReturn{false};
        bool isBarcode{false};
        bool hasExplicitUnit{false};
        domain::SaleUnitSelection unit{domain::SaleUnitSelection::PieceOrTablet};
    };

    static ParsedCommand parseInput(const QString& input);

signals:
    void itemSelected(const domain::Item& item, int qty, domain::SaleUnitSelection unit);
    void commandEntered(const ui::PosSearchBox::ParsedCommand& cmd);
    void barcodeEntered(const QString& barcode);
    void searchEntered(const QString& text);
    void navigateToCartRequested();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void moveEvent(QMoveEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void handleTextEdited(const QString& text);
    void handlePopupItemChosen(const domain::Item& item);

private:
    void updatePopupPosition();
    void commitSearchCommand();
    void ensurePopup();

    PosSearchPopup* m_popup{nullptr};
    bool m_isProcessingCommand{false};
};

} // namespace ui
