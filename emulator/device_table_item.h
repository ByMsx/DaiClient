#ifndef DEVICETABLEITEM_H
#define DEVICETABLEITEM_H

#include <QModbusServer>
#include <Dai/device.h>
#include <Dai/type_managers.h>
#include "devices_table_item.h"

class DeviceTableItem : public DevicesTableItem
{
    bool enabled_;
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
