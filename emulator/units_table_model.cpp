#include "units_table_model.h"
#include <Dai/deviceitem.h>
#include "Dai/typemanager/typemanager.h"

#include <algorithm>

Units_Table_Model::Units_Table_Model(Dai::ItemTypeManager *mng, const QVector<Dai::DeviceItem *> *units_vector, QObject *parent)
    : QAbstractItemModel(parent), item_type_manager_(mng)
{    
    for (auto *item_unit : *units_vector)
    {
        modbus_units_map_[static_cast<QModbusDataUnit::RegisterType>(item_type_manager_->registerType(item_unit->type()))].push_back(item_unit);
    }

    for (auto &it: modbus_units_map_)
    {
        auto sort_units = [](const Dai::DeviceItem *a, const Dai::DeviceItem *b) -> bool
        {
            return a->unit().toInt() < b->unit().toInt();
        };
        std::sort(it.second.begin(), it.second.end(), sort_units);

        int max_unit = it.second.back()->unit().toInt();
        auto it_t = it.second.begin();
        for (int i = 0; i <= max_unit; ++i)
        {
            if (it_t == it.second.end() || (*it_t)->unit() != i)
            {
                it_t = it.second.insert(it_t, nullptr);
            }
            ++it_t;
        }
    }
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
        auto ip = parent.internalId();
        if (ip > 5)
            return 0;

        auto reg_type = row_to_register_type(parent.row());
        if (reg_type == QModbusDataUnit::Invalid)
        {
            return 0;
        }
        return modbus_units_map_.at(reg_type).size();
    }
    else
    {
        return 4;
    }
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
                if (role == Qt::CheckStateRole)
                {
                    return static_cast<int>(Qt::Checked);
                }
                else if (role == Qt::DisplayRole)
                {
                    return device_item->unit();
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
                    auto reg_type = static_cast<QModbusDataUnit::RegisterType>(item_type_manager_->registerType(device_item->type()));
                    if (role == UNIT_TYPE_ROLE)
                    {
                        return reg_type;
                    }
                    if (reg_type > QModbusDataUnit::Coils)
                    {
                        if (role == Qt::EditRole || role == Qt::DisplayRole)
                        {
                            if (!device_item->getRawValue().isNull())
                            {
                                return device_item->getRawValue();
                            }
                            return 0;
                        }
                    }
                    else if (reg_type > QModbusDataUnit::Invalid)
                    {
                        if (role == Qt::CheckStateRole)
                        {
                            bool result = false;
                            if (!device_item->getRawValue().isNull())
                            {
                                result = (device_item->getRawValue() >0);//== 1);
                            }
                            return static_cast<int>(result ? Qt::Checked : Qt::Unchecked);
//                            return result;
                        }
                    }
                    else
                    {
                        if (role == Qt::DisplayRole)
                        {
                            return device_item->getRawValue();
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

    Qt::ItemFlags flags = Qt::ItemIsEnabled;// | Qt::ItemIsSelectable;

    auto reg_type = static_cast<QModbusDataUnit::RegisterType>(item_type_manager_->registerType(device_item->type()));

    if (index.column() == 0)
    {
        flags |= Qt::ItemIsUserCheckable;
    }
    else if (index.column() == 2)
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

            auto reg_type = static_cast<QModbusDataUnit::RegisterType>(item_type_manager_->registerType(device_item->type()));
            if (role == Qt::CheckStateRole)
            {
                if (reg_type > QModbusDataUnit::Invalid && reg_type <= QModbusDataUnit::Coils)
                {
                    device_item->setRawValue(value);
                    emit dataChanged(index, index);
                    return true;
                }

            }
            else if (role == Qt::EditRole)
            {
                if (reg_type > QModbusDataUnit::Coils)
                {
                    device_item->setRawValue(value);
                    emit dataChanged(index, index);
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
        auto ip = parent.internalId();
        if (ip > 5)
            return QModelIndex();

        if (column < 0 || column > 5 || row < 0)
        {
            return QModelIndex();
        }
        auto reg_type = row_to_register_type(parent.row());

        auto it = modbus_units_map_.find(reg_type);
        if (it == modbus_units_map_.cend() || row > it->second.size())
        {
            return QModelIndex();
        }
        return createIndex(row, column, it->second.at(row));

    }
    else if (row >= 0 && row < 4 && column == 0)
    {
        return createIndex(row, column, row_to_register_type(row));
    }
    return QModelIndex();
}

QModelIndex Units_Table_Model::parent(const QModelIndex &child) const
{
    if (!child.isValid())
    {
        return QModelIndex();
    }

    auto ip = child.internalId();
    if (ip > 0 && ip < 5)
        return QModelIndex();

    auto device_item = static_cast<Dai::DeviceItem*>(child.internalPointer());
    if (device_item)
    {
        auto reg_type = static_cast<QModbusDataUnit::RegisterType>(item_type_manager_->registerType(device_item->type()));
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
