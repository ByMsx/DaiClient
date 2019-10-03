#ifndef DEVICETABLEMODEL_H
#define DEVICETABLEMODEL_H

#include <QAbstractTableModel>
#include <QVector>
#include <QModbusServer>
#include <QModbusDataUnit>

#include <Dai/device.h>
#include "registers_vector_item.h"
#include "device_table_item.h"

class DevicesTableModel : public QAbstractItemModel
{
    enum Column
    {
        UNIT_TYPE = 0,
        UNIT_NAME,
        UNIT_VALUE
    };

    Q_OBJECT
private:
    typedef std::vector<Dai::DeviceItem*> Device_Items_Vector;
    typedef QVector<Dai::Device*> Devices_Vector;

//    std::map<QModbusDataUnit::RegisterType, Device_Items_Vector> modbus_units_map_;
//    Devices_Vector modbus_devices_vector_;

    QVector<DeviceTableItem*> modbus_devices_vector_;
    Dai::Database::Item_Type_Manager* item_type_manager_;

    void add_items(const Devices_Vector* devices, QModbusServer* modbus_server);
public:
    DevicesTableModel(Dai::Database::Item_Type_Manager* mng, const QVector<Dai::Device *> *devices_vector, QModbusServer *modbus_server, QObject *parent = nullptr);
    DevicesTableModel(Dai::Database::Item_Type_Manager* mng, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child = QModelIndex()) const override;

    void appendChild(DeviceTableItem* item);
};

#endif // DEVICETABLEMODEL_H
