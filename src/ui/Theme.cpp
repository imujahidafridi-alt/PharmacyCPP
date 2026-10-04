#include "ui/Theme.h"

namespace ui {

QString Theme::getAppStyleSheet()
{
    return QString(R"(
        QWidget {
            background-color: #F8FAFC;
            color: #0F172A;
            font-family: 'Segoe UI', 'Inter', -apple-system, sans-serif;
            font-size: 12px;
        }

        /* Global Dialog Background */
        QDialog {
            background-color: #FFFFFF;
        }

        /* Compact Navigation Bar (Replaces Top Header) */
        #navBar {
            background-color: #FFFFFF;
            border-bottom: 1px solid #CBD5E1;
            min-height: 36px;
            max-height: 36px;
            padding: 0px 6px;
        }

        QPushButton.navBtn {
            background-color: transparent;
            color: #475569;
            border: none;
            border-bottom: 2px solid transparent;
            border-radius: 0px;
            padding: 7px 12px;
            font-weight: 600;
            font-size: 12px;
            min-height: 20px;
        }

        QPushButton.navBtn:hover {
            background-color: #F1F5F9;
            color: #0F766E;
        }

        QPushButton.navBtn:checked {
            border-bottom: 2px solid #0F766E;
            color: #0F766E;
            background-color: #F0FDFA;
            font-weight: 700;
        }

        /* Compact Status Meta on Right of Nav */
        #navMetaBadge {
            background-color: #F1F5F9;
            color: #475569;
            border: 1px solid #E2E8F0;
            border-radius: 3px;
            padding: 3px 8px;
            font-size: 11px;
            font-weight: 600;
        }

        /* Flat, Small Buttons */
        QPushButton {
            background-color: #0F766E;
            color: #FFFFFF;
            border: 1px solid #0D5F59;
            border-radius: 3px;
            padding: 4px 10px;
            font-weight: 600;
            font-size: 12px;
            min-height: 24px;
            outline: none;
        }

        QPushButton:hover {
            background-color: #115E59;
            border-color: #115E59;
        }

        QPushButton:pressed {
            background-color: #134E4A;
            border-color: #134E4A;
        }

        QPushButton:disabled {
            background-color: #E2E8F0;
            border-color: #CBD5E1;
            color: #94A3B8;
        }

        /* Secondary Outline Button */
        QPushButton.secondaryBtn,
        QPushButton[class="secondaryBtn"] {
            background-color: #FFFFFF;
            color: #334155;
            border: 1px solid #CBD5E1;
            border-radius: 3px;
            padding: 4px 10px;
            font-weight: 600;
            font-size: 12px;
            min-height: 24px;
            outline: none;
        }

        QPushButton.secondaryBtn:hover,
        QPushButton[class="secondaryBtn"]:hover {
            background-color: #F8FAFC;
            border-color: #94A3B8;
            color: #0F172A;
        }

        QPushButton.secondaryBtn:pressed,
        QPushButton[class="secondaryBtn"]:pressed {
            background-color: #F1F5F9;
        }

        /* Danger Button */
        QPushButton.dangerBtn,
        QPushButton[class="dangerBtn"] {
            background-color: #FFFFFF;
            border: 1px solid #FCA5A5;
            color: #DC2626;
            border-radius: 3px;
            padding: 4px 10px;
            font-weight: 600;
            font-size: 12px;
            min-height: 24px;
            outline: none;
        }

        QPushButton.dangerBtn:hover,
        QPushButton[class="dangerBtn"]:hover {
            background-color: #FEE2E2;
            border-color: #DC2626;
            color: #B91C1C;
        }

        QPushButton.dangerBtn:pressed,
        QPushButton[class="dangerBtn"]:pressed {
            background-color: #FECACA;
        }

        /* Info Button */
        QPushButton.infoBtn,
        QPushButton[class="infoBtn"] {
            background-color: #0284C7;
            color: #FFFFFF;
            border: 1px solid #0369A1;
            border-radius: 3px;
            padding: 4px 10px;
            font-weight: 600;
            font-size: 12px;
            min-height: 24px;
            outline: none;
        }

        QPushButton.infoBtn:hover,
        QPushButton[class="infoBtn"]:hover {
            background-color: #0369A1;
            border-color: #0284C7;
        }

        QPushButton.infoBtn:pressed,
        QPushButton[class="infoBtn"]:pressed {
            background-color: #075985;
        }

        /* Success Button */
        QPushButton.successBtn,
        QPushButton[class="successBtn"] {
            background-color: #16A34A;
            color: #FFFFFF;
            border: 1px solid #15803D;
            border-radius: 3px;
            padding: 4px 10px;
            font-weight: 600;
            font-size: 12px;
            min-height: 24px;
            outline: none;
        }

        QPushButton.successBtn:hover,
        QPushButton[class="successBtn"]:hover {
            background-color: #15803D;
            border-color: #16A34A;
        }

        QPushButton.successBtn:pressed,
        QPushButton[class="successBtn"]:pressed {
            background-color: #166534;
        }

        /* Flat Primary Action Button */
        QPushButton.primaryBtn,
        QPushButton[class="primaryBtn"],
        #primaryCheckoutBtn {
            background-color: #0F766E;
            color: #FFFFFF;
            border: 1px solid #0D5F59;
            border-radius: 3px;
            font-size: 12px;
            font-weight: 700;
            padding: 4px 14px;
            min-height: 26px;
            outline: none;
        }

        QPushButton.primaryBtn:hover,
        QPushButton[class="primaryBtn"]:hover,
        #primaryCheckoutBtn:hover {
            background-color: #115E59;
            border-color: #115E59;
        }

        QPushButton.primaryBtn:pressed,
        QPushButton[class="primaryBtn"]:pressed,
        #primaryCheckoutBtn:pressed {
            background-color: #134E4A;
            border-color: #134E4A;
        }

        /* Compact Flat Input Controls */
        QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox, QDateEdit {
            background-color: #FFFFFF;
            border: 1px solid #CBD5E1;
            border-radius: 3px;
            padding: 4px 8px;
            min-height: 22px;
            font-size: 12px;
            color: #0F172A;
            selection-background-color: #0F766E;
            selection-color: #FFFFFF;
        }

        QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus, QDateEdit:focus {
            border: 1px solid #0F766E;
            background-color: #FFFFFF;
        }

        /* Input Error State */
        QLineEdit[hasError="true"],
        QComboBox[hasError="true"],
        QSpinBox[hasError="true"] {
            border: 1.5px solid #DC2626;
            background-color: #FEF2F2;
        }

        /* App Dropdown Popup List View */
        QListView#appDropdownPopup {
            background-color: #FFFFFF;
            border: 1.5px solid #0F766E;
            border-radius: 4px;
            padding: 3px;
            outline: none;
            font-size: 12px;
            color: #1E293B;
            selection-background-color: #CCFBF1;
            selection-color: #0F766E;
        }

        QListView#appDropdownPopup::item {
            padding: 5px 8px;
            min-height: 22px;
            border-radius: 2px;
            border-bottom: 1px solid #F1F5F9;
        }

        QListView#appDropdownPopup::item:hover {
            background-color: #F0FDFA;
            color: #0F766E;
        }

        QListView#appDropdownPopup::item:selected {
            background-color: #0F766E;
            color: #FFFFFF;
            font-weight: 600;
        }

        /* App Search Box */
        #appSearchBox {
            background-color: #FFFFFF;
            border: 1px solid #CBD5E1;
            border-radius: 3px;
            padding: 4px 8px;
            min-height: 22px;
            font-size: 12px;
            color: #0F172A;
            selection-background-color: #0F766E;
            selection-color: #FFFFFF;
        }

        #appSearchBox:focus {
            border: 1.5px solid #0F766E;
        }

        /* POS Compact Search Bar */
        #posSearchBox {
            font-size: 13px;
            padding: 6px 10px;
            border: 1.5px solid #0F766E;
            border-radius: 3px;
            background-color: #FFFFFF;
            min-height: 26px;
            max-height: 30px;
        }

        /* Search Completer Popup Dropdown */
        QListView#searchCompleterPopup,
        QCompleter QAbstractItemView {
            background-color: #FFFFFF;
            border: 1.5px solid #0F766E;
            border-radius: 4px;
            padding: 4px;
            outline: none;
            font-size: 12px;
            color: #1E293B;
            selection-background-color: #CCFBF1;
            selection-color: #0F766E;
        }

        QListView#searchCompleterPopup::item,
        QCompleter QAbstractItemView::item {
            padding: 6px 10px;
            min-height: 24px;
            border-radius: 3px;
            border-bottom: 1px solid #F1F5F9;
            color: #1E293B;
        }

        QListView#searchCompleterPopup::item:hover,
        QCompleter QAbstractItemView::item:hover {
            background-color: #F0FDFA;
            color: #0F766E;
        }

        QListView#searchCompleterPopup::item:selected,
        QCompleter QAbstractItemView::item:selected {
            background-color: #0F766E;
            color: #FFFFFF;
            font-weight: 600;
        }

        /* Enterprise High-Density Tables */
        QTableView, QTableWidget, DataTable {
            background-color: #FFFFFF;
            alternate-background-color: #F8FAFC;
            gridline-color: #E2E8F0;
            border: 1px solid #CBD5E1;
            border-radius: 3px;
            selection-background-color: #CCFBF1;
            selection-color: #0F766E;
            font-size: 12px;
            color: #0F172A;
            outline: none;
        }

        QTableView::item, QTableWidget::item {
            padding: 2px 8px;
            border: none;
            border-bottom: 1px solid #F1F5F9;
        }

        QTableView::item:hover, QTableWidget::item:hover {
            background-color: #F1F5F9;
        }

        QTableView::item:selected, QTableWidget::item:selected {
            background-color: #CCFBF1;
            color: #0F766E;
            font-weight: 600;
        }

        QHeaderView {
            background-color: #F1F5F9;
            border: none;
        }

        QHeaderView::section {
            background-color: #F8FAFC;
            color: #475569;
            font-weight: 700;
            font-size: 11px;
            border: none;
            border-right: 1px solid #E2E8F0;
            border-bottom: 2px solid #CBD5E1;
            padding: 5px 8px;
            text-transform: uppercase;
            letter-spacing: 0.5px;
        }

        QHeaderView::section:hover {
            background-color: #E2E8F0;
            color: #0F172A;
        }

        QTableCornerButton::section {
            background-color: #F8FAFC;
            border: none;
            border-bottom: 2px solid #CBD5E1;
            border-right: 1px solid #E2E8F0;
        }

        /* Compact Summary Strip */
        #totalDisplayCard {
            background-color: #FFFFFF;
            border: 1px solid #CBD5E1;
            border-radius: 3px;
            padding: 6px 12px;
            min-height: 34px;
            max-height: 42px;
        }

        #totalDisplayAmount {
            color: #0F766E;
            font-size: 22px;
            font-weight: 800;
        }

        /* Scrollbars */
        QScrollBar:vertical {
            background: #F8FAFC;
            width: 7px;
            margin: 0px;
        }
        QScrollBar::handle:vertical {
            background: #CBD5E1;
            min-height: 18px;
            border-radius: 3px;
        }
        QScrollBar::handle:vertical:hover {
            background: #94A3B8;
        }
    )");
}

} // namespace ui
