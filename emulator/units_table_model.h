#ifndef UNITS_TABLE_MODEL_H
#define UNITS_TABLE_MODEL_H

#include <QAbstractTableModel>
#include <map>
#include <QModbusDataUnit>

namespace Dai
{
class DeviceItem;
struct ItemTypeManager;
}

class QModbusServer;

class Units_Table_Model : public QAbstractItemModel
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
    std::map<QModbusDataUnit::RegisterType, Device_Items_Vector> modbus_units_map_;
    Dai::ItemTypeManager* item_type_manager_;
    QModbusServer *modbus_server_;

public:
    explicit Units_Table_Model(Dai::ItemTypeManager* mng, const QVector<Dai::DeviceItem *> *units_vector, QModbusServer *modbus_server, QObject *parent = 0);

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    Qt::ItemFlags flags(const QModelIndex &index) const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole);
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child = QModelIndex()) const override;

    const static int UNIT_TYPE_ROLE = Qt::DisplayRole + 1;

private slots:
    void update_table_values(QModbusDataUnit::RegisterType type, int address, int size) noexcept;

};

#endif // UNITS_TABLE_MODEL_H
