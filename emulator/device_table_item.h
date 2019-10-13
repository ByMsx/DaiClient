#ifndef DEVICETABLEITEM_H
#define DEVICETABLEITEM_H

#include <QModbusServer>
#include <Dai/device.h>
#include <Dai/type_managers.h>
#include <vector>
#include <map>
#include "devices_table_item.h"

class RegisterTableItem;

class DeviceTableItem : public DevicesTableItem
{
    Q_OBJECT

    bool enabled_;
    QModbusServer* modbus_server_;

    RegisterTableItem* get_child_register_item_by_type(QModbusDataUnit::RegisterType type) const;

    void init_by_register_type(Dai::Item_Type_Manager *mng, QModbusServer* modbus_server, Dai::Device *device, QModbusDataUnit::RegisterType registerType);
    void update_modbus_server_map();

    QModbusDataUnitMap generate_modbus_data_unit_map() const;
signals:
    void data_changed(DeviceTableItem*);
public slots:
    void update_table_values(QModbusDataUnit::RegisterType type, int address, int size);
public:
    DeviceTableItem(Dai::Item_Type_Manager *mng, QModbusServer* modbus_server, Dai::Device *device, DevicesTableItem* parent = nullptr);
    ~DeviceTableItem() override = default;

    void assign(Dai::Device *device);

    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    bool isUse() const noexcept;
    void toggle() noexcept;
};

#endif // DEVICETABLEITEM_H
