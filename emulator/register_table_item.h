#ifndef REGISTERTABLEITEM_H
#define REGISTERTABLEITEM_H

#include "devices_table_item.h"
#include "registers_vector_item.h"

class RegisterTableItem : public DevicesTableItem
{
public:
    RegisterTableItem(RegistersVectorItem* data, DevicesTableItem* parent = nullptr);
    ~RegisterTableItem() override = default;

    QVariant data(const QModelIndex &index, int role) const override;
};

#endif // REGISTERTABLEITEM_H
