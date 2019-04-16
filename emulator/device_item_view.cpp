#include "device_item_view.h"

#include <Dai/deviceitem.h>
#include <Dai/device.h>
#include "units_table_model.h"
//#include "units_table_delegate.h"

#include <QGroupBox>
#include <QTreeView>
#include <QVBoxLayout>
#include <QtWidgets/QHeaderView>
#include "Dai/type_managers.h"

Device_Item_View::Device_Item_View(Dai::Item_Type_Manager *mng, Dai::Device *dev, QModbusServer *modbus_server, QWidget *parent) : QWidget(parent), item_type_manager_(mng), device_(dev), modbus_server_(modbus_server)
{
    init();
}

void Device_Item_View::init() noexcept
{
    QVBoxLayout* center_layout = new QVBoxLayout(this);
    setLayout(center_layout);

    device_group_box_ = new QGroupBox(device_->param("address").toString(), this);
    device_group_box_->setTitle(device_->name());
    device_group_box_->setCheckable(true);
    device_group_box_->setChecked(true);

    center_layout->addWidget(device_group_box_);

    units_tree_view_ = new QTreeView(this);
    QVBoxLayout* table_layout = new QVBoxLayout;
    table_layout->addWidget(units_tree_view_);
    table_layout->setMargin(0);
    device_group_box_->setLayout(table_layout);

    if (device_)
    {
        units_table_model_ = new Units_Table_Model(item_type_manager_, &device_->items(), modbus_server_, this);
        units_tree_view_->setModel(units_table_model_);

        units_tree_view_->expandAll();
        for (int column = 0; column < units_table_model_->columnCount(); ++column)
        {
            units_tree_view_->resizeColumnToContents(column);
        }

    }

    QObject::connect(device_group_box_, &QGroupBox::clicked, [&](bool value)
    {
        units_tree_view_->setEnabled(value);
    });
}

void Device_Item_View::add_device(Dai::Device *dev) noexcept
{
    if (units_table_model_ != nullptr)
    {
        units_table_model_->add_items(&dev->items());
        units_tree_view_->reset();
        units_tree_view_->expandAll();
    }
}

bool Device_Item_View::is_use() const noexcept
{
    return device_group_box_->isChecked();
}
