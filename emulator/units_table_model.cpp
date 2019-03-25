#include "units_table_model.h"
#include <Dai/deviceitem.h>
#include "Dai/type_managers.h"
#include "Dai/section.h"

#include <QModbusServer>
#include <QDebug>

#include <algorithm>

int unit(const Dai::DeviceItem *a)
{
    auto it = a->extra().find("unit");
    return it != a->extra().cend() ? it.value().toInt() : 0;
}

Units_Table_Model::Units_Table_Model(Dai::Item_Type_Manager *mng, const QVector<Dai::DeviceItem *> *units_vector, QModbusServer *modbus_server, QObject *parent)
    : QAbstractItemModel(parent), item_type_manager_(mng), modbus_server_(modbus_server)
{    
    for (auto *item_unit : *units_vector)
    {
        if (item_unit->type_id() == Dai::Prt::itWindowState)
            item_unit->setRawValue(Dai::Prt::wCalibrated | Dai::Prt::wExecuted | Dai::Prt::wClosed);
        else
            item_unit->setRawValue(mng->need_normalize(item_unit->type_id()) ? (qrand() % 100) + 240 : (qrand() % 3000) + 50);

        modbus_units_map_[static_cast<QModbusDataUnit::RegisterType>(item_type_manager_->register_type(item_unit->type_id()))].push_back(item_unit);
    }

    QModbusDataUnitMap modbus_data_unit_map;
    for (auto &it: modbus_units_map_)
    {
        auto sort_units = [](const Dai::DeviceItem *a, const Dai::DeviceItem *b) -> bool
        {
            return unit(a) < unit(b);
        };
        std::sort(it.second.begin(), it.second.end(), sort_units);

        int max_unit = unit(it.second.back());
        auto it_t = it.second.begin();
        for (int i = 0; i <= max_unit; ++i)
        {
            if (it_t == it.second.end() || unit(*it_t) != i)
            {
                it_t = it.second.insert(it_t, nullptr);
            }
            ++it_t;
        }

        QModbusDataUnit modbus_unit(it.first, 0, it.second.size()); // max_unit

        if (it.first > QModbusDataUnit::Coils)
        {
            for (uint i = 0; i < modbus_unit.valueCount(); ++i)
            {
                if (it.second.at(i))
                {
                    modbus_unit.setValue(i, it.second.at(i)->raw_value().toInt());
                }
            }

        }

        modbus_data_unit_map.insert(it.first, modbus_unit);
    }
    modbus_server_->setMap(modbus_data_unit_map);
    QObject::connect(modbus_server_, &QModbusServer::dataWritten, this, &Units_Table_Model::update_table_values);
}


QModbusDataUnit::RegisterType row_to_register_type(int row)
{
    switch (row) {
    case 0: return QModbusDataUnit::DiscreteInputs;
    case 1: return QModbusDataUnit::Coils;
    case 2: return QModbusDataUnit::InputRegisters;
    case 3: return QModbusDataUnit::HoldingRegisters;
    default:
        break;
    }
    return QModbusDataUnit::Invalid;
}

int Units_Table_Model::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
    {
        if (parent.internalPointer() == nullptr)
        {
            auto reg_type = row_to_register_type(parent.row());
            if (reg_type != QModbusDataUnit::Invalid)
            {
                auto it = modbus_units_map_.find(reg_type);
                if (it != modbus_units_map_.cend())
                {
                    return it->second.size();
                }
            }
        }
    }
    else
    {
        return 4;
    }
    return 0;
}


int Units_Table_Model::columnCount(const QModelIndex &parent) const
{
    return 3;
}

QVariant Units_Table_Model::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole)
    {
        if (orientation == Qt::Horizontal)
        {
            switch (section)
            {
                case Column::UNIT_TYPE:
                    return "Type";
                case Column::UNIT_NAME:
                    return "Name";
                case Column::UNIT_VALUE:
                    return "Value";
            }
        }
        else
        {
            return QString("%1").arg(section + 1);
        }
    }

    return QVariant();
}

QVariant Units_Table_Model::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
    {
        return QVariant();
    }

    if (index.parent().isValid())
    {
        auto device_item = dynamic_cast<Dai::DeviceItem*>((QObject*)index.internalPointer());

        if (!device_item)
        {
            return QVariant();
        }

        switch (index.column())
        {
            case Column::UNIT_TYPE:
                if (role == Qt::DisplayRole)
                {
                    return unit(device_item);
                }
                break;
            case Column::UNIT_NAME:
                if (role == Qt::DisplayRole)
                {
                    QString name("%1  %2");
                    return name.arg(device_item->id()).arg(device_item->displayName());
                }
                break;
            case Column::UNIT_VALUE:
                {
                    auto reg_type = static_cast<QModbusDataUnit::RegisterType>(item_type_manager_->register_type(device_item->type_id()));
                    if (reg_type > QModbusDataUnit::Coils)
                    {
                        if (role == Qt::EditRole || role == Qt::DisplayRole)
                        {
                            if (!device_item->raw_value().isNull())
                            {
                                return device_item->raw_value();
                            }
                            return 0;
                        }
                    }
                    else if (reg_type > QModbusDataUnit::Invalid)
                    {
                        if (role == Qt::CheckStateRole)
                        {
                            bool result = false;
                            if (!device_item->raw_value().isNull())
                            {
                                result = (device_item->raw_value() > 0 && device_item->raw_value() <= 2);
                            }
                            return static_cast<int>(result ? Qt::Checked : Qt::Unchecked);
                        }
                    }
                    else
                    {
                        if (role == Qt::DisplayRole)
                        {
                            return device_item->raw_value();
                        }
                    }
                }
        }
    }
    else
    {
        if (role == Qt::DisplayRole)
        {
            switch (index.row())
            {
                case 0: return "Discrete Inputs";
                case 1: return "Coils";
                case 2: return "Input Registers";
                case 3: return "Holding Registers";
                default:
                    break;
            }
        }
    }

    return QVariant();
}

Qt::ItemFlags Units_Table_Model::flags(const QModelIndex &index) const // not works
{
    if (!index.isValid() || !index.parent().isValid())
    {
        return Qt::ItemIsEnabled;
    }

    auto device_item = dynamic_cast<Dai::DeviceItem*>((QObject*)index.internalPointer());

    if (!device_item)
    {
        return Qt::NoItemFlags;
    }

    Qt::ItemFlags flags = Qt::ItemIsEnabled;

    auto reg_type = static_cast<QModbusDataUnit::RegisterType>(item_type_manager_->register_type(device_item->type_id()));

    if (index.column() == 2)
    {
        if (reg_type > QModbusDataUnit::Coils)
        {
            flags |= Qt::ItemIsEditable;
        }
        else if (reg_type > QModbusDataUnit::Invalid)
        {
            flags |= Qt::ItemIsUserCheckable;
        }
    }

    return flags;
}

bool Units_Table_Model::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (index.isValid() && index.parent().isValid())
    {
        if (index.column() == 2)
        {
            auto device_item = dynamic_cast<Dai::DeviceItem*>((QObject*)index.internalPointer());
            if (!device_item)
            {
                return false;
            }

            auto reg_type = static_cast<QModbusDataUnit::RegisterType>(item_type_manager_->register_type(device_item->type_id()));
            if (role == Qt::CheckStateRole)
            {
                if (reg_type > QModbusDataUnit::Invalid && reg_type <= QModbusDataUnit::Coils)
                {
                    device_item->setRawValue(value);
                    emit dataChanged(index, index);
                    modbus_server_->setData(reg_type, unit(device_item), value.toBool());
                    return true;
                }

            }
            else if (role == Qt::EditRole)
            {
                if (reg_type > QModbusDataUnit::Coils)
                {
                    device_item->setRawValue(value);
                    emit dataChanged(index, index);
                    modbus_server_->setData(reg_type, unit(device_item), value.toInt());
                    return true;
                }
            }
        }


    }

    return false;
}


QModelIndex Units_Table_Model::index(int row, int column, const QModelIndex &parent) const
{
    if (parent.isValid())
    {
        if (parent.internalPointer() != nullptr)
        {
            return QModelIndex();
        }

        if (column < 0 || column > 3 || row < 0)
        {
            return QModelIndex();
        }
        auto reg_type = row_to_register_type(parent.row());

        auto it = modbus_units_map_.find(reg_type);
        if (it == modbus_units_map_.cend() || row > it->second.size() || it->second.at(row) == nullptr)
        {
            return QModelIndex();
        }
        return createIndex(row, column, it->second.at(row));

    }
    else if (row >= 0 && row < 4 && column == 0)
    {
        return createIndex(row, column);
    }
    return QModelIndex();
}

QModelIndex Units_Table_Model::parent(const QModelIndex &child) const
{
    if (!child.isValid() || child.internalPointer() == nullptr)
    {
        return QModelIndex();
    }

    auto device_item = static_cast<Dai::DeviceItem*>(child.internalPointer());
    if (device_item)
    {
        auto reg_type = static_cast<QModbusDataUnit::RegisterType>(item_type_manager_->register_type(device_item->type_id()));
        int row;
        switch (reg_type) {
        case QModbusDataUnit::DiscreteInputs       : row = 0;  break;
        case QModbusDataUnit::Coils                : row = 1;  break;
        case QModbusDataUnit::InputRegisters       : row = 2;  break;
        case QModbusDataUnit::HoldingRegisters     : row = 3;  break;
        default:
            return QModelIndex();
        }

        return createIndex(row, 0);
    }
    return QModelIndex();
}

void Units_Table_Model::update_table_values(QModbusDataUnit::RegisterType type, int address, int size) noexcept
{
//    qDebug() << "Update data" << type << address << size;
    for (int i = 0; i < size; ++i)
    {
        if (type == QModbusDataUnit::Coils || type == QModbusDataUnit::HoldingRegisters)
        {
            quint16 value;
            modbus_server_->data(type, address + i, &value);

            if (auto dev_item = modbus_units_map_.at(type).at(address + i))
            {
                dev_item->setRawValue(value);
            }
        }
    }

    emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1));
}

