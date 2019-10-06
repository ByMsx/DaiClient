#ifndef REGISTERTABLEITEM_H
#define REGISTERTABLEITEM_H

#include "devices_table_item.h"
#include "registers_vector_item.h"

class RegisterTableItem : public DevicesTableItem
{
public:
    RegisterTableItem(RegistersVectorItem* data, DevicesTableItem* parent = nullptr);
    ~RegisterTableItem() override = default;

    void assign(const QVector<Dai::DeviceItem*>& items);

    QVariant data(const QModelIndex &index, int role) const override;
private:
    void append_childs(const QVector<Dai::DeviceItem*>& items);
};

#endif // REGISTERTABLEITEM_H
