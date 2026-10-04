#pragma once
#include <QStyledItemDelegate>
#include <QLineEdit>
#include <QIntValidator>
#include <QDoubleValidator>
#include <QKeyEvent>
#include <functional>
#include <vector>
#include "domain/Models.h"

namespace ui {

class CartTableDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    using CartProvider = std::function<const std::vector<domain::CartItem>&()>;

    explicit CartTableDelegate(CartProvider cartProvider, QObject* parent = nullptr)
        : QStyledItemDelegate(parent), m_cartProvider(std::move(cartProvider)) {}

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& /*option*/, const QModelIndex& index) const override {
        int row = index.row();
        int col = index.column();
        const auto& cart = m_cartProvider();
        if (row < 0 || row >= static_cast<int>(cart.size())) {
            return nullptr;
        }
        const auto& item = cart[row];

        // EDGE CASE 1: The "Non-Discountable" Lock (FMCG / Cosmetics / Baby Milk)
        // Strictly block inline editing of Discount % (Col 5) and Net Price (Col 6) for FMCG items
        if ((col == 5 || col == 6) && !item.isDiscountable) {
            emit nonDiscountableBlocked(item.itemName);
            return nullptr; // Hard block: no editor created!
        }

        auto* editor = new QLineEdit(parent);
        editor->setAlignment(col == 6 ? (Qt::AlignRight | Qt::AlignVCenter) : Qt::AlignCenter);
        editor->setStyleSheet(
            "QLineEdit {"
            "  background-color: #FFFFFF;"
            "  color: #0F172A;"
            "  font-weight: 700;"
            "  font-size: 13px;"
            "  border: 2px solid #0F766E;"
            "  border-radius: 3px;"
            "  padding: 1px;"
            "}"
        );

        if (col == 3) {
            editor->setValidator(new QIntValidator(-9999, 9999, editor));
        } else if (col == 5) {
            editor->setValidator(new QDoubleValidator(0.0, 100.0, 2, editor));
        } else if (col == 6) {
            editor->setValidator(new QDoubleValidator(0.0, 999999.0, 2, editor));
        }

        editor->installEventFilter(const_cast<CartTableDelegate*>(this));
        return editor;
    }

    void setEditorData(QWidget* editor, const QModelIndex& index) const override {
        auto* lineEdit = qobject_cast<QLineEdit*>(editor);
        if (!lineEdit) return;
        int row = index.row();
        int col = index.column();
        const auto& cart = m_cartProvider();
        if (row < 0 || row >= static_cast<int>(cart.size())) return;
        const auto& item = cart[row];

        if (col == 3) {
            lineEdit->setText(QString::number(item.displayQty));
        } else if (col == 5) {
            lineEdit->setText(QString::number(item.discountPct, 'f', (std::fmod(item.discountPct, 1.0) == 0.0 ? 0 : 2)));
        } else if (col == 6) {
            lineEdit->setText(item.unitSalePrice.formatted(false));
        }
        lineEdit->selectAll();
    }

    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override {
        auto* lineEdit = qobject_cast<QLineEdit*>(editor);
        if (!lineEdit) return;
        QString text = lineEdit->text().trimmed();
        if (!text.isEmpty()) {
            model->setData(index, text, Qt::EditRole);
        }
    }

    void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex& /*index*/) const override {
        editor->setGeometry(option.rect);
    }

    bool eventFilter(QObject* object, QEvent* event) override {
        auto* editor = qobject_cast<QLineEdit*>(object);
        if (editor && event->type() == QEvent::KeyPress) {
            auto* keyEvent = static_cast<QKeyEvent*>(event);
            if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
                const_cast<CartTableDelegate*>(this)->commitData(editor);
                const_cast<CartTableDelegate*>(this)->closeEditor(editor);
                keyEvent->accept();
                return true;
            }
        }
        return QStyledItemDelegate::eventFilter(object, event);
    }

signals:
    void nonDiscountableBlocked(const QString& itemName) const;

private:
    CartProvider m_cartProvider;
};

} // namespace ui
