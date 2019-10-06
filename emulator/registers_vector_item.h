#ifndef REGISTERS_VECTOR_ITEM_H
#define REGISTERS_VECTOR_ITEM_H

#include <QObject>
#include <Dai/deviceitem.h>
#include <Dai/type_managers.h>
#include <QModbusDataUnit>
#include <QModbusServer>

class RegistersVectorItem : public QObject {
    Q_OBJECT

    QVector<Dai::DeviceItem*> items_;
    QModbusDataUnit::RegisterType type_;
    QModbusServer* modbus_server_;
    Dai::Database::Item_Type_Manager* itemTypeManager_;
public:
    RegistersVectorItem(Dai::Database::Item_Type_Manager* mng, QModbusServer* modbus_server, QModbusDataUnit::RegisterType type, const QVector<Dai::DeviceItem*>& items, QObject* parent = nullptr);
    QVector<Dai::DeviceItem*> assign(const QVector<Dai::DeviceItem*>& items);

    QModbusDataUnit::RegisterType type() const;
    QVector<Dai::DeviceItem*> items() const;
    Dai::Database::Item_Type_Manager* manager() const;
    QModbusServer* modbus_server() const;
};

#endif // REGISTERS_VECTOR_ITEM_H
