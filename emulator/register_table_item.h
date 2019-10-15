#ifndef REGISTERTABLEITEM_H
#define REGISTERTABLEITEM_H

#include "devices_table_item.h"
#include "registers_vector_item.h"

class RegisterTableItem : public DevicesTableItem
{
public:
    static bool use_favorites_only();
    static void set_use_favorites_only(bool use_favorites_only);

    RegisterTableItem(RegistersVectorItem* data, DevicesTableItem* parent = nullptr);
    ~RegisterTableItem() override = default;

    void assign(const QVector<Dai::DeviceItem*>& items);

    QVariant data(const QModelIndex &index, int role) const override;

    DevicesTableItem* child(int row) const override;
    int childCount() const override;
private:
    void append_childs(const QVector<Dai::DeviceItem*>& items);

    static bool use_favorites_only_;
};

#endif // REGISTERTABLEITEM_H
