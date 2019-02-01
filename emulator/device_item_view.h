#ifndef DEVICE_ITEM_VIEW_H
#define DEVICE_ITEM_VIEW_H

#include <QWidget>
#include <QModbusDataUnit>
#include <vector>

class QGroupBox;
class QTreeView;
class Units_Table_Model;

namespace Dai {
class Device;
struct ItemTypeManager;
class DeviceItem;
}

class Device_Item_View : public QWidget
{
    Q_OBJECT
private:
    QGroupBox* device_group_box_;
    QTreeView* units_tree_view_;
    Units_Table_Model* units_table_model_;

    Dai::Device* device_;
    Dai::ItemTypeManager* item_type_manager_;

public:
    explicit Device_Item_View(Dai::ItemTypeManager* mng, Dai::Device* dev, QWidget *parent = nullptr);

    bool is_use() const noexcept;

private:
    void init() noexcept;

signals:

public slots:
};

#endif // DEVICE_ITEM_VIEW_H
