#include "device_item_table_item.h"
#include <QModbusDataUnit>

int unit(const Dai::DeviceItem *a)
{
    if (a == nullptr)
    {
        return -1;
    }
    QVariant val = a->param("unit");
    return val.isValid() ? val.toInt() : -1;
}

DeviceItemTableItem::DeviceItemTableItem(Dai::Database::Item_Type_Manager *mng, QModbusServer* modbus_server, Dai::DeviceItem *item, DevicesTableItem *parent)
    : DevicesTableItem(item, parent), item_type_manager_(mng), modbus_server_(modbus_server)
{

}

QVariant DeviceItemTableItem::data(const QModelIndex &index, int role) const {
    if (index.isValid()) {
        const Dai::DeviceItem* device_item = static_cast<Dai::DeviceItem*>(this->itemData_);

        switch (index.column()) {
        case UNIT_TYPE:
            if (role == Qt::DisplayRole) {
                return unit(device_item);
            }
            break;
        case UNIT_NAME: {
            if (role == Qt::DisplayRole) {
                QString name("%1  %2");
                return name.arg(device_item->id()).arg(device_item->displayName());
            }
            break;
        }
        case UNIT_VALUE: {
            auto reg_type = static_cast<QModbusDataUnit::RegisterType>(item_type_manager_->register_type(device_item->type_id()));
            if (reg_type > QModbusDataUnit::Coils)
            {
                if (role == Qt::EditRole || role == Qt::DisplayRole)
                {
                    if (!device_item->raw_value().isNull())
                    {
                        return device_item->raw_value();
                    }
                    return 0;
                }
            }
            else if (reg_type > QModbusDataUnit::Invalid)
            {
                if (role == Qt::CheckStateRole)
                {
                    bool result = false;
                    if (!device_item->raw_value().isNull())
                    {
                        result = device_item->raw_value().toBool();//( > 0);// && device_item->raw_value() <= 2);
                    }
                    return static_cast<int>(result ? Qt::Checked : Qt::Unchecked);
                }
            }
            else
            {
                if (role == Qt::DisplayRole)
                {
                    return device_item->raw_value();
                }
            }
        }
        }
    }

    return QVariant();
}

Qt::ItemFlags DeviceItemTableItem::flags(const QModelIndex& index) const {
    Qt::ItemFlags flags = Qt::ItemIsEnabled;

    if (index.isValid() && index.column() == 2) {
        auto item = static_cast<Dai::DeviceItem*>(this->itemData_);
        auto register_type = static_cast<QModbusDataUnit::RegisterType>(item_type_manager_->register_type(item->type_id()));
        if (register_type > QModbusDataUnit::Coils) {
            flags |= Qt::ItemIsEditable;
        } else if (register_type > QModbusDataUnit::Invalid) {
            flags |= Qt::ItemIsUserCheckable;
        }
    }

    return flags;
};

QModbusServer* DeviceItemTableItem::modbus_server() const {
    return this->modbus_server_;
}
