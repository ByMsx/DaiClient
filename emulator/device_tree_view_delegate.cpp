#include "device_tree_view_delegate.h"
#include <QLineEdit>

DeviceTreeViewDelegate::DeviceTreeViewDelegate(QTreeView *parent) : QStyledItemDelegate(parent) {}

QWidget *DeviceTreeViewDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    if (DeviceTreeViewDelegate::is_our_cell(index))
    {
        QLineEdit* line_edit = new QLineEdit(parent);
        line_edit->setClearButtonEnabled(true);
        return line_edit;
    }

    return QStyledItemDelegate::createEditor(parent, option, index);
}

bool DeviceTreeViewDelegate::is_our_cell(const QModelIndex &index)
{
    return index.data(Qt::UserRole) == 1;
}
