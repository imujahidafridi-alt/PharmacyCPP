#pragma once
#include <QStyledItemDelegate>
#include <QLineEdit>
#include <QIntValidator>
#include <QKeyEvent>

namespace ui {

class CartQtyDelegate : public QStyledItemDelegate {
public:
    explicit CartQtyDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& /*option*/, const QModelIndex& /*index*/) const override {
        auto* editor = new QLineEdit(parent);
        editor->setAlignment(Qt::AlignCenter);
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
        auto* validator = new QIntValidator(-9999, 9999, editor);
        editor->setValidator(validator);
        editor->installEventFilter(const_cast<CartQtyDelegate*>(this));
        return editor;
    }

    void setEditorData(QWidget* editor, const QModelIndex& index) const override {
        auto* lineEdit = qobject_cast<QLineEdit*>(editor);
        if (lineEdit) {
            lineEdit->setText(index.data(Qt::DisplayRole).toString());
            lineEdit->selectAll();
        }
    }

    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override {
        auto* lineEdit = qobject_cast<QLineEdit*>(editor);
        if (lineEdit) {
            QString text = lineEdit->text().trimmed();
            if (!text.isEmpty()) {
                model->setData(index, text, Qt::EditRole);
            }
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
                const_cast<CartQtyDelegate*>(this)->commitData(editor);
                const_cast<CartQtyDelegate*>(this)->closeEditor(editor);
                keyEvent->accept();
                return true; // Consume event completely so it doesn't propagate to window
            }
        }
        return QStyledItemDelegate::eventFilter(object, event);
    }
};

} // namespace ui
