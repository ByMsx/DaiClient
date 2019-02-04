#include "units_table_model.h"
#include <Dai/deviceitem.h>
#include "Dai/typemanager/typemanager.h"
#include "Dai/section.h"

#include <QModbusServer>
#include <QDebug>

#include <algorithm>

Units_Table_Model::Units_Table_Model(Dai::ItemTypeManager *mng, const QVector<Dai::DeviceItem *> *units_vector, QModbusServer *modbus_server, QObject *parent)
    : QAbstractItemModel(parent), item_type_manager_(mng), modbus_server_(modbus_server)
{    
    for (auto *item_unit : *units_vector)
    {
        if (item_unit->type() == Dai::Prt::itWindowState)
            item_unit->setRawValue(Dai::Prt::wCalibrated | Dai::Prt::wExecuted | Dai::Prt::wClosed);
        else
            item_unit->setRawValue(mng->needNormalize(item_unit->type()) ? (qrand() % 100) + 240 : (qrand() % 3000) + 50);

        modbus_units_map_[static_cast<QModbusDataUnit::RegisterType>(item_type_manager_->registerType(item_unit->type()))].push_back(item_unit);
    }

    QModbusDataUnitMap modbus_data_unit_map;
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

        QModbusDataUnit modbus_unit(it.first, 0, it.second.size()); // max_unit

        // For what?
//        if (it.first > 2)
//        {
//            for (uint i = 0; i < modbus_unit.valueCount(); ++i)
//            {
//                auto w_it = widgets.find(it.second.at(i));
//                if (w_it != widgets.cend())
//                    modbus_unit.setValue(i, static_cast<QSpinBox*>(w_it->second)->value());
//            }

//        }

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
//                            return device_item->getRawValue();
                            bool result = false;
                            if (!device_item->getRawValue().isNull())
                            {
                                result = (device_item->getRawValue() > 0 && device_item->getRawValue() <= 2);
                            }
                            return static_cast<int>(result ? Qt::Checked : Qt::Unchecked);
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

    Qt::ItemFlags flags = Qt::ItemIsEnabled;

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
                    modbus_server_->setData(reg_type, device_item->unit().toInt(), value.toBool());
                    return true;
                }

            }
            else if (role == Qt::EditRole)
            {
                if (reg_type > QModbusDataUnit::Coils)
                {
                    device_item->setRawValue(value);
                    emit dataChanged(index, index);
                    modbus_server_->setData(reg_type, device_item->unit().toInt(), value.toInt());
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
        if (it == modbus_units_map_.cend() || row > it->second.size() || it->second.at(row) == nullptr)
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


//        switch (type)
//        {
//            case QModbusDataUnit::Coils:
//            {
//                modbus_server_->data(QModbusDataUnit::Coils, address + i, &value);

//                if (auto dev_item = modbus_units_map_.at(type).at(address + i))
//                {
//                    dev_item->setRawValue(value);
////                    auto it = widgets.find(dev_item);
////                    if (it != widgets.cend())
////                        if (auto box = qobject_cast<QCheckBox*>(it->second))
////                        {
////                            box->setChecked(value);
////                            break;
////                        }
//                }
//                break;
//            }
//            case QModbusDataUnit::HoldingRegisters:
//            {
//                modbus_server_->data(QModbusDataUnit::HoldingRegisters, address + i, &value);

//                auto dev_item = modbus_server_.at(type).at(address + i);
//                if (auto dev_item = modbus_units_map_.at(type).at(address + i))
//                {
//                    dev_item->setRawValue(value);
////                    auto it = widgets.find(dev_item);
////                    if (it != widgets.cend())
////                    {
////                        if (auto spin = qobject_cast<QSpinBox*>(it->second))
////                        {
////                            spin->setValue(value);
////                            break;
////                        }
////                    }
//                }
//                break;
//            }
//            default: break;
//        }
    }

    emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1));
}

//void Units_Table_Model::set_coil_value(bool value)
//{
//    set_device_value(QModbusDataUnit::Coils, value);
//}

//void Units_Table_Model::set_register_value(int value)
//{
//    if (static_cast<QSpinBox*>(sender())->property("IsHolding").toBool())
//        setDeviceValue(QModbusDataUnit::HoldingRegisters, value);
//    else
//        setDeviceValue(QModbusDataUnit::InputRegisters, value);
//}

//void Units_Table_Model::set_device_value(QModbusDataUnit::RegisterType table, quint16 value, QWidget *w)
//{
//    if (!w)
//        w = static_cast<QWidget*>(sender());

//    auto dev_item_it = std::find_if(widgets.cbegin(), widgets.cend(), [w](auto it) {
//        return it.second == w;
//    });

//    if (dev_item_it != widgets.cend())
//    {
//        auto& items = m_items.at(table);
//        auto dev_item_in_items = std::find(items.cbegin(), items.cend(), dev_item_it->first);
//        if (dev_item_in_items != items.cend())
//        {
//            std::size_t index = std::distance(items.cbegin(), dev_item_in_items);
//            _modbus_->setData(table, index, value);
//        }
//    }
//}
