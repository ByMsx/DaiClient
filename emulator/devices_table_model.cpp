#include "devices_table_model.h"
#include "device_item_table_item.h"
#include "register_table_item.h"

#include <Dai/deviceitem.h>
#include <Dai/type_managers.h>
#include <Dai/section.h>

DevicesTableModel::DevicesTableModel(Dai::Database::Item_Type_Manager* mng, const QVector<Dai::Device *> *devices_vector, QModbusServer *modbus_server, QObject* parent)
    : QAbstractItemModel(parent), item_type_manager_(mng)
{
    add_items(devices_vector, modbus_server);
}

DevicesTableModel::DevicesTableModel(Dai::Database::Item_Type_Manager *mng, QObject *parent)
    : QAbstractItemModel(parent), item_type_manager_(mng)
{

}

int DevicesTableModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        DevicesTableItem* internalPtr = static_cast<DevicesTableItem*>(parent.internalPointer());
        if (!internalPtr) {
            return 0;
        }
        return internalPtr->childCount();
    }

    return this->modbus_devices_vector_.count();
}

int DevicesTableModel::columnCount(const QModelIndex &parent) const
{
    return 3;
}

QVariant DevicesTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole)
    {
        if (orientation == Qt::Horizontal)
        {
            switch (section)
            {
                case DeviceTableItem::Column::UNIT_TYPE:
                    return "Type";
                case DeviceTableItem::Column::UNIT_NAME:
                    return "Name";
                case DeviceTableItem::Column::UNIT_VALUE:
                    return "Value";
            }
        } else {
            return QString("%1").arg(section + 1);
        }
    }

    return QVariant();
}

Qt::ItemFlags DevicesTableModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return Qt::ItemIsEnabled;
    }

    DevicesTableItem* internalPtr = static_cast<DevicesTableItem*>(index.internalPointer());
    return internalPtr->flags(index);
}

QVariant DevicesTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    DevicesTableItem* internalPtr = static_cast<DevicesTableItem*>(index.internalPointer());
    if (!internalPtr) {
        return QVariant();
    }

    return internalPtr->data(index, role);
}

bool DevicesTableModel::setData(const QModelIndex &index, const QVariant &value, int role) {
    if (index.isValid())
    {
        if (index.column() == 0 && role == Qt::CheckStateRole) {
            auto deviceTableItem = static_cast<DeviceTableItem*>(index.internalPointer());
            if (!deviceTableItem) {
                return false;
            }

            if (deviceTableItem->isUse() != value.toBool()) {
                deviceTableItem->toggle();
                emit dataChanged(index, index);
            }
        }
        if (index.column() == 2 && index.parent().isValid())
        {
            auto deviceTableItem = static_cast<DeviceItemTableItem*>(index.internalPointer());
            if (!deviceTableItem) {
                return false;
            }

            auto device_item = dynamic_cast<Dai::DeviceItem*>(deviceTableItem->item());

            auto reg_type = static_cast<QModbusDataUnit::RegisterType>(item_type_manager_->register_type(device_item->type_id()));
            if (role == Qt::CheckStateRole)
            {
                if (reg_type > QModbusDataUnit::Invalid && reg_type <= QModbusDataUnit::Coils)
                {
                    device_item->set_raw_value(value);
                    emit dataChanged(index, index);
                    deviceTableItem->modbus_server()->setData(reg_type, unit(device_item), value.toBool());
                    return true;
                }

            }
            else if (role == Qt::EditRole)
            {
                if (reg_type > QModbusDataUnit::Coils)
                {
                    device_item->set_raw_value(value);
                    emit dataChanged(index, index);
                    deviceTableItem->modbus_server()->setData(reg_type, unit(device_item), value.toInt());
                    return true;
                }
            }
        }
    }

    return false;
}

QModelIndex DevicesTableModel::index(int row, int column, const QModelIndex &parent) const {
    if (parent.isValid()) {
        if (column < 0 || column > 3 || row < 0) {
            return QModelIndex();
        }

        DevicesTableItem* parentPtr = static_cast<DevicesTableItem*>(parent.internalPointer());
        DevicesTableItem* childPtr = parentPtr->child(row);

        return createIndex(row, column, childPtr);
    }

    if (column == 0 && modbus_devices_vector_.size() > row) {
        DevicesTableItem* ptr = this->modbus_devices_vector_.at(row);
        return createIndex(row, column, ptr);
    }
    return QModelIndex();
}

QModelIndex DevicesTableModel::parent(const QModelIndex &child) const
{
    if (!child.isValid()) {
        return QModelIndex();
    }

    DevicesTableItem* itemPtr = static_cast<DevicesTableItem*>(child.internalPointer());
    if (!itemPtr) {
        return QModelIndex();
    }

    DevicesTableItem* parentPtr = itemPtr->parent();
    if (!parentPtr) {
        return QModelIndex();
    }
    return createIndex(parentPtr->row(), 0, parentPtr);
}

void DevicesTableModel::appendChild(DeviceTableItem *item) {
    connect(item, &DeviceTableItem::data_changed, this, &DevicesTableModel::child_item_changed);

    this->modbus_devices_vector_.push_back(item);
    QModelIndex index = createIndex(this->modbus_devices_vector_.count() - 2, 0, item);
    emit dataChanged(index, index);
}

bool DevicesTableModel::set_is_favorite(const QModelIndex &index, bool state)
{
    if (index.isValid() && index.parent().isValid())
    {
        auto deviceTableItem = dynamic_cast<DeviceItemTableItem*>(static_cast<DevicesTableItem*>(index.internalPointer()));
        if (deviceTableItem)
        {
            if (RegisterTableItem::use_favorites_only())
            {
                beginResetModel();
                deviceTableItem->set_is_favorite(state);
                endResetModel();
                return true;
            }
            else
            {
                deviceTableItem->set_is_favorite(state);
            }
        }
    }
    return false;
}

void DevicesTableModel::set_use_favorites_only(bool use_favorites_only)
{
    beginResetModel();
    RegisterTableItem::set_use_favorites_only(use_favorites_only);
    endResetModel();
}

void DevicesTableModel::add_items(const Devices_Vector *devices, QModbusServer* modbus_server) {
    for (auto& device : *devices) {
        this->modbus_devices_vector_.push_back(new DeviceTableItem(item_type_manager_, modbus_server, device));
    }
}

void DevicesTableModel::child_item_changed(DeviceTableItem *child_item)
{
    // TODO: here we can optimize if needed
    emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1));
}
