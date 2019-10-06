#include "registers_vector_item.h"

RegistersVectorItem::RegistersVectorItem(Dai::Database::Item_Type_Manager* mng,
                                         QModbusServer* modbus_server,
                                         QModbusDataUnit::RegisterType type,
                                         const QVector<Dai::DeviceItem*>& items,
                                         QObject* parent): QObject(parent), type_(type), itemTypeManager_(mng), modbus_server_(modbus_server)
{
    assign(items);
}

QVector<Dai::DeviceItem*> RegistersVectorItem::assign(const QVector<Dai::DeviceItem*>& items)
{
    QVector<Dai::DeviceItem*> result;
    for (const auto& ptr : items) {
        if (itemTypeManager_->register_type(ptr->type_id()) == type_) {
            items_.push_back(ptr);
            result.push_back(ptr);
        }
    }
    return result;
}

QModbusDataUnit::RegisterType RegistersVectorItem::type() const {
    return this->type_;
}

QVector<Dai::DeviceItem*> RegistersVectorItem::items() const {
    return this->items_;
}

Dai::Database::Item_Type_Manager* RegistersVectorItem::manager() const {
    return this->itemTypeManager_;
}

QModbusServer* RegistersVectorItem::modbus_server() const {
    return this->modbus_server_;
}
