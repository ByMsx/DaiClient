#ifndef DEVICE_ITEM_VIEW_H
#define DEVICE_ITEM_VIEW_H

#include <QWidget>
#include <QModbusDataUnit>
#include <vector>

class QGroupBox;
class QTreeView;
class Units_Table_Model;
class QModbusServer;
//class Units_Table_Delegate;

namespace Dai {
namespace Database {
struct Item_Type_Manager;
} // namespace Database
class Device;
class DeviceItem;
} // namespace Dai

class Device_Item_View : public QWidget
{
    Q_OBJECT
private:
    QGroupBox *device_group_box_;
    QTreeView *units_tree_view_;
    Units_Table_Model *units_table_model_;
//    Units_Table_Delegate *units_table_delegate_;

    Dai::Device *device_;
    Dai::Database::Item_Type_Manager *item_type_manager_;
    QModbusServer *modbus_server_;

public:
    explicit Device_Item_View(Dai::Database::Item_Type_Manager* mng, Dai::Device* dev, QModbusServer *modbus_server, QWidget *parent = nullptr);

    bool is_use() const noexcept;

private:
    void init() noexcept;

signals:

public slots:
};

#endif // DEVICE_ITEM_VIEW_H
