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
//    int full_count = 0;
//    for (const auto &item : modbus_units_map_)
//    {
//        full_count += item.second.size();
//    }

//    return full_count;
}

Units_Table_Model::Device_Items_Vector *Units_Table_Model::get_item(const QModelIndex &index) const
{
    if (index.isValid())
    {
        auto *item = static_cast<Device_Items_Vector*>(index.internalPointer());
        if (item)
        {
            return item;
        }
    }
    return {};//rootItem;
}


int Units_Table_Model::columnCount(const QModelIndex &parent) const
{
//    if (parent.isValid())
//    {
//        return 5;
//    }
//    return 1;
    return 5;
}

QVariant Units_Table_Model::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole)
    {
        if (orientation == Qt::Horizontal)
        {
            switch (section)
            {
                case Header::UNIT_TYPE:
                    return "Type";
                case Header::UNIT_IS_ACTIVE:
                    return "V";
                case Header::UNIT_NAME:
                    return "Name";
                case Header::UNIT_VALUE:
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

    if (role != Qt::DisplayRole && role != Qt::EditRole)
    {
        return QVariant();
    }


//    qobject_cast<Dai::DeviceItem*>((QObject*)index.internalPointer());
    if (index.parent().isValid())
    {
        auto device_items = dynamic_cast<Dai::DeviceItem*>((QObject*)index.internalPointer());
        if (device_items)
        {
            switch (index.column())
            {
                case Header::UNIT_IS_ACTIVE:
                    return "V";
                case Header::UNIT_TYPE:
                    return device_items->unit();
                case Header::UNIT_NAME:
                {
                    QString name("%1 %2");
                    return name.arg(device_items->id()).arg(device_items->displayName());
                }
                case Header::UNIT_VALUE:
                    return device_items->getRawValue();
    //                if (units_vector_->at(index.row())->type() < 3)
    //                {
    //                    return units_vector_->at(index.row())->getValue();
    //                }
    //                else
    //                {

    //                }
            }
//            return device_items->unit();
        }

    }
    else
    {
//        QModbusDataUnit::RegisterType reg_type = static_cast<QModbusDataUnit::RegisterType>();
        switch (index.row()) {
        case 0: return "DiscreteInputs";
        case 1: return "Coils";
        case 2: return "InputRegisters";
        case 3: return "HoldingRegisters";
        default:
            break;
        }
    }

    return QVariant();



//    if (!modbus_units_map_)
//    {
//        return QVariant();
//    }

//    if (role == Qt::DisplayRole)
//    {

//        if (index.column() == 0)
//            return modbus_units_map_->
//        if (index.column() == 1)
//            return modbus_units_map_->values().at(index.row());
//        QModbusDataUnit::RegisterType

//        switch (index.column())
//        {
//            case Header::UNIT_TYPE:
//            if (item_type_manager_)
//            {
//                modbus_units_map_->at()
//                return item_type_manager_->registerType(units_vector_->at(index.row())->type());
//            }
//            case Header::UNIT_IS_ACTIVE:
//                return "V";
//            case Header::UNIT_ID:
//                return units_vector_->at(index.row())->unit();
//            case Header::UNIT_NAME:
//            {
//                QString name("%1 %2");
//                return name.arg(units_vector_->at(index.row())->id()).arg(units_vector_->at(index.row())->displayName());
//            }
//            case Header::UNIT_VALUE:
//                return units_vector_->at(index.row())->getValue();
////                if (units_vector_->at(index.row())->type() < 3)
////                {
////                    return units_vector_->at(index.row())->getValue();
////                }
////                else
////                {

////                }
//        }


//    }
    return QVariant();
}

bool Units_Table_Model::setData(const QModelIndex &index, const QVariant &value, int role)
{
    return false;
}


QModelIndex Units_Table_Model::index(int row, int column, const QModelIndex &parent) const
{
//    Device_Items_Vector *item = static_cast<Device_Items_Vector*>(parent.internalPointer());

//    if (!item)
//        return QModelIndex();


//    TreeItem *childItem = item->at(row);
//    if (childItem)
//        return createIndex(row, column, childItem);
//    else
//        return QModelIndex();

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
