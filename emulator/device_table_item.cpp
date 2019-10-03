#include "device_table_item.h"
#include "register_table_item.h"
#include "registers_vector_item.h"

#include <QDebug>

DeviceTableItem::DeviceTableItem(Dai::Item_Type_Manager *mng, QModbusServer* modbus_server, Dai::Device* device, DevicesTableItem *parent)
    : DevicesTableItem(device, parent), enabled_(true)
{
    RegistersVectorItem* coils = new RegistersVectorItem(mng, modbus_server, QModbusDataUnit::RegisterType::Coils, device->items());
    RegistersVectorItem* discreteInputs = new RegistersVectorItem(mng, modbus_server, QModbusDataUnit::RegisterType::DiscreteInputs, device->items());
    RegistersVectorItem* inputs = new RegistersVectorItem(mng, modbus_server, QModbusDataUnit::RegisterType::InputRegisters, device->items());
    RegistersVectorItem* holdings = new RegistersVectorItem(mng, modbus_server, QModbusDataUnit::RegisterType::HoldingRegisters, device->items());

    RegisterTableItem* coilsTableItem = new RegisterTableItem(coils, this);
    RegisterTableItem* discreteInputsItem = new RegisterTableItem(discreteInputs, this);
    RegisterTableItem* inputsItem = new RegisterTableItem(inputs, this);
    RegisterTableItem* holdingsItem = new RegisterTableItem(holdings, this);

    this->appendChild(coilsTableItem);
    this->appendChild(discreteInputsItem);
    this->appendChild(inputsItem);
    this->appendChild(holdingsItem);
}

Qt::ItemFlags DeviceTableItem::flags(const QModelIndex &index) const {
    Qt::ItemFlags flags = Qt::ItemFlag::ItemIsEnabled;

    if (index.isValid() && index.column() == 0) {
        flags |= Qt::ItemFlag::ItemIsUserCheckable;
    }

    return flags;
}

QVariant DeviceTableItem::data(const QModelIndex &index, int role) const {
    if (!index.isValid()) return QVariant();

    if (index.column() == 0) {
        if (role == Qt::DisplayRole) {
            return "Устройство " + static_cast<Dai::Device*>(this->itemData_)->param("address").toString();
        } else if (role == Qt::CheckStateRole) {
            return static_cast<int>(enabled_ ? Qt::Checked : Qt::Unchecked);
        }
    }

    return QVariant();
}

bool DeviceTableItem::isUse() const noexcept {
    return this->enabled_;
}

void DeviceTableItem::toggle() noexcept {
    this->enabled_ = !this->enabled_;
}
