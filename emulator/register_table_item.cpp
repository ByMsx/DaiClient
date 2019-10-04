#include "register_table_item.h"
#include "device_item_table_item.h"

RegisterTableItem::RegisterTableItem(RegistersVectorItem* data, DevicesTableItem* parent)
    : DevicesTableItem(data, parent)
{
    for (auto& item : data->items()) {
        this->appendChild(new DeviceItemTableItem(data->manager(), data->modbus_server(), item, this));
    }
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
