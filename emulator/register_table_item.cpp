#include "register_table_item.h"
#include "device_item_table_item.h"

RegisterTableItem::RegisterTableItem(RegistersVectorItem* data, DevicesTableItem* parent)
    : DevicesTableItem(data, parent)
{
    append_childs(data->items());
}

void RegisterTableItem::assign(const QVector<Dai::DeviceItem*>& items)
{
    const QVector<Dai::DeviceItem*> assigned_vect = static_cast<RegistersVectorItem*>(this->itemData_)->assign(items);
    append_childs(assigned_vect);
}

QVariant RegisterTableItem::data(const QModelIndex &index, int role) const {
    if (index.isValid() && role == Qt::DisplayRole) {
        if (index.column() == 0) {
            auto register_type = static_cast<RegistersVectorItem*>(this->itemData_)->type();
            switch (register_type) {
                case QModbusDataUnit::RegisterType::Coils:
                    return "Coils";
                case QModbusDataUnit::RegisterType::DiscreteInputs:
                    return "Discrete Inputs";
                case QModbusDataUnit::RegisterType::InputRegisters:
                    return "Input Registers";
                case QModbusDataUnit::RegisterType::HoldingRegisters:
                    return "Holding Registers";
                default:
                    return QVariant();
            }
        }
    }

    return QVariant();
}

void RegisterTableItem::append_childs(const QVector<Dai::DeviceItem*>& items)
{
    auto data = static_cast<RegistersVectorItem*>(this->itemData_);
    for (auto& item : items)
    {
        this->appendChild(new DeviceItemTableItem(data->manager(), data->modbus_server(), item, this));
    }
}
