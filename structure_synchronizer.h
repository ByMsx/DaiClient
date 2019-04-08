#ifndef DAI_CLIENT_STRUCTURE_SYNCHRONIZER_H
#define DAI_CLIENT_STRUCTURE_SYNCHRONIZER_H

#include <Helpz/net_protocol.h>

#include <Dai/project.h>
#include <Dai/db/group_status_item.h>
#include <plus/dai/structure_synchronizer.h>

namespace Dai {
namespace Client {

class Protocol_2_0;

class Structure_Synchronizer : public QObject, public Dai::Structure_Synchronizer
{
    Q_OBJECT
public:
    Structure_Synchronizer();

    void set_project(Project* project);

    void send_status_insert(uint32_t user_id, const Group_Status_Item& item);
    void send_status_update(uint32_t user_id, const Group_Status_Item& item);
    void send_status_delete(uint32_t user_id, uint32_t group_status_id);

public slots:
    void set_protocol(std::shared_ptr<Client::Protocol_2_0> protocol = {});
    void send_project_structure(uint8_t struct_type, uint8_t msg_id, QIODevice* data_dev);
private:
    void send_modify_response(const QByteArray &buffer) override;
    void add_checker_types(QDataStream& ds);
    void add_device_items(QDataStream& ds);
    void add_groups(QDataStream& ds);
    void add_group_status_items(QDataStream& ds);
    void add_device_item_values(QDataStream& ds);
    void add_group_mode(QDataStream& ds);
    void add_views(QDataStream& ds);
    void add_view_itemss(QDataStream& ds);
    void add_codes(const QVector<uint32_t>& ids, QDataStream* ds);

    Project* prj_;
    std::shared_ptr<Protocol_2_0> protocol_;
};

} // namespace Client
} // namespace Dai

#endif // DAI_CLIENT_STRUCTURE_SYNCHRONIZER_H
