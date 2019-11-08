#include "register_table_item.h"
#include "device_item_table_item.h"

/*static*/ bool RegisterTableItem::use_favorites_only_ = false;
/*static*/ bool RegisterTableItem::use_favorites_only()
{
    return use_favorites_only_;
}

/*static*/ void RegisterTableItem::set_use_favorites_only(bool use_favorites_only)
{
    use_favorites_only_ = use_favorites_only;
}

RegisterTableItem::RegisterTableItem(RegistersVectorItem* data, DevicesTableItem* parent)
    : DevicesTableItem(data, parent)
{
    append_childs(data->items());
}

void RegisterTableItem::assign(const QVector<Dai::DeviceItem*>& items)
{
    const QVector<Dai::DeviceItem*> assigned_vect = static_cast<RegistersVectorItem*>(this->itemData_)->assign(items);
    append_childs(assigned_vect);
}

QVariant RegisterTableItem::data(const QModelIndex &index, int role) const {
    if (!index.isValid()) return QVariant();

    if (role == Qt::DisplayRole) {
        if (index.column() == 0) {
            auto register_type = static_cast<RegistersVectorItem*>(this->itemData_)->type();
            switch (register_type) {
                case QModbusDataUnit::RegisterType::Coils:
                    return "Coils";
                case QModbusDataUnit::RegisterType::DiscreteInputs:
                    return "Discrete Inputs";
                case QModbusDataUnit::RegisterType::InputRegisters:
                    return "Input Registers";
                case QModbusDataUnit::RegisterType::HoldingRegisters:
                    return "Holding Registers";
                default:
                    return QVariant();
            }
        }
    }

    return QVariant();
}

DevicesTableItem *RegisterTableItem::child(int row) const
{
    if (use_favorites_only())
    {
        int child_count = DevicesTableItem::childCount();

        DeviceItemTableItem* child;
        int favorites_child_i_ = 0;
        for (int i = 0; i < child_count; ++i)
        {
            child = dynamic_cast<DeviceItemTableItem*>(DevicesTableItem::child(i));
            if (child && child->is_favorite() && favorites_child_i_++ == row)
            {
                return child;
            }
        }
    }
    return DevicesTableItem::child(row);
}

int RegisterTableItem::childCount() const
{
    int child_count = DevicesTableItem::childCount();
    if (use_favorites_only() && child_count)
    {
        DeviceItemTableItem* child;
        int favorites_child_count_ = 0;
        for (int i = 0; i < child_count; ++i)
        {
            child = dynamic_cast<DeviceItemTableItem*>(DevicesTableItem::child(i));
            if (child && child->is_favorite())
            {
                ++favorites_child_count_;
            }
        }
        return favorites_child_count_;
    }
    return child_count;
}

void RegisterTableItem::append_childs(const QVector<Dai::DeviceItem*>& items)
{
    auto data = static_cast<RegistersVectorItem*>(this->itemData_);
    for (auto& item : items)
    {
        this->appendChild(new DeviceItemTableItem(data->manager(), data->modbus_server(), item, this));
    }
}
