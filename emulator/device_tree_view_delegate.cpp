#include "device_tree_view_delegate.h"
#include "devices_table_model.h"

#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QEvent>
#include <QDebug>

DeviceTreeViewDelegate::DeviceTreeViewDelegate(QTreeView *parent)
    : QStyledItemDelegate(parent), parent_widget_(parent)
{
    this->btn_ = new QPushButton("script", this->parent_widget_);
    this->btn_->hide();
}

QWidget *DeviceTreeViewDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    if (DeviceTreeViewDelegate::is_input_or_holding_register_value_cell(index))
    {
        QLineEdit* line_edit = new QLineEdit(parent);
        line_edit->setClearButtonEnabled(true);
        return line_edit;
    }

    return QStyledItemDelegate::createEditor(parent, option, index);
}

void DeviceTreeViewDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    if (DeviceTreeViewDelegate::is_device_value_cell(index))
    {
        this->btn_->setGeometry(option.rect);
        painter->drawPixmap(option.rect.x(), option.rect.y(), this->btn_->grab());
    }
    else
    {
        QStyledItemDelegate::paint(painter, option, index);
    }
}

bool DeviceTreeViewDelegate::editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index)
{
    if (DeviceTreeViewDelegate::is_device_value_cell(index) && event->type() == QEvent::Type::MouseButtonPress)
    {
        QMouseEvent* mouse_event = dynamic_cast<QMouseEvent*>(event);
        if (mouse_event && mouse_event->button() == Qt::LeftButton)
        {
            DevicesTableModel* model = qobject_cast<DevicesTableModel*>(this->parent_widget_->model());
            if (model)
            {
                model->emit_call_script(index);
                return true;
            }
        }
    }

    return QStyledItemDelegate::editorEvent(event, model, option, index);
}

bool DeviceTreeViewDelegate::is_input_or_holding_register_value_cell(const QModelIndex &index)
{
    return index.data(Qt::UserRole) == 1;
}

bool DeviceTreeViewDelegate::is_device_value_cell(const QModelIndex& index)
{
    return index.data(Qt::UserRole) == 2;
}
