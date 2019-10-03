#ifndef DEVICEITEMTABLEITEM_H
#define DEVICEITEMTABLEITEM_H

#include "devices_table_item.h"
#include <Dai/deviceitem.h>
#include <Dai/type_managers.h>
#include <QModbusServer>

class DeviceItemTableItem : public DevicesTableItem
{
    enum Column
    {
        UNIT_TYPE = 0,
        UNIT_NAME,
        UNIT_VALUE
    };

    Dai::Database::Item_Type_Manager* item_type_manager_;
    QModbusServer* modbus_server_;
public:
    DeviceItemTableItem(Dai::Database::Item_Type_Manager *mng, QModbusServer* modbus_server, Dai::DeviceItem* item, DevicesTableItem* parent = nullptr);
    ~DeviceItemTableItem() override = default;

    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QModbusServer* modbus_server() const;
};

int unit(const Dai::DeviceItem *a);

#endif // DEVICEITEMTABLEITEM_H
