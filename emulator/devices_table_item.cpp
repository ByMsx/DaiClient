#include "devices_table_item.h"

DevicesTableItem::DevicesTableItem(QObject *data, const QVector<DevicesTableItem *> &children, DevicesTableItem *parent)
    : child_(children), parent_(parent), itemData_(data) {

}

DevicesTableItem::DevicesTableItem(QObject *data, DevicesTableItem *parent)
    : parent_(parent), itemData_(data)
{

}

DevicesTableItem::DevicesTableItem(DevicesTableItem *parent)
    : parent_(parent)
{
}

Qt::ItemFlags DevicesTableItem::flags(const QModelIndex &index) const {
    return Qt::ItemIsEnabled;
}

DevicesTableItem *DevicesTableItem::parent() const {
    return this->parent_;
}

DevicesTableItem *DevicesTableItem::child(int row) const {
    return this->get_child(row);
}

DevicesTableItem *DevicesTableItem::get_child(int row) const {
    return this->child_.value(row);
}

bool DevicesTableItem::hasChild() const {
    return this->childCount() > 0;
}

int DevicesTableItem::childCount() const {
    return this->get_child_count();
}

int DevicesTableItem::get_child_count() const {
    return this->child_.count();
}

int DevicesTableItem::row() const {
    if (!this->parent()) {
        return 0;
    }

    return this->parent()->child_.indexOf(const_cast<DevicesTableItem*>(this));
}

void DevicesTableItem::appendChild(DevicesTableItem *item) {
    this->child_.push_back(item);
}

QObject* DevicesTableItem::item() const {
    return this->itemData_;
}
