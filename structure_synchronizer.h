#ifndef DAI_CLIENT_STRUCTURE_SYNCHRONIZER_H
#define DAI_CLIENT_STRUCTURE_SYNCHRONIZER_H

#include <Helpz/net_protocol.h>

#include <Dai/project.h>
#include <Dai/db/group_status_item.h>
#include <plus/dai/structure_synchronizer.h>

namespace Dai {
namespace Client {

class Protocol;

class Structure_Synchronizer : public QObject, public Dai::Structure_Synchronizer
{
    Q_OBJECT
public:
    Structure_Synchronizer(Helpz::Database::Thread *db_thread);

    void set_project(Project* project);

    void send_status_insert(uint32_t user_id, const Group_Status_Item& item);
    void send_status_update(uint32_t user_id, const Group_Status_Item& item);
    void send_status_delete(uint32_t user_id, uint32_t group_status_id);

signals:
    void client_modified(uint32_t user_id);
public slots:
    void set_protocol(std::shared_ptr<Client::Protocol> protocol = {});
    void send_project_structure(uint8_t struct_type, uint8_t msg_id, QIODevice* data_dev);
private:
    void send_structure_items_hash(uint8_t struct_type, uint8_t msg_id);
    void add_structure_items_hash(uint8_t struct_type, QDataStream& ds, Helpz::Database::Base& db);

    void send_structure_items(const QVector<uint32_t>& id_vect, uint8_t struct_type, uint8_t msg_id);

    void send_structure_hash(uint8_t struct_type, uint8_t msg_id, Helpz::Database::Base& db);
    void send_structure_hash_for_all(uint8_t msg_id, Helpz::Database::Base& db);
    void send_structure(uint8_t struct_type, uint8_t msg_id, Helpz::Database::Base& db);

    void send_modify_response(uint8_t struct_type, const QByteArray &buffer, uint32_t user_id) override;

    Project* prj_;
    std::shared_ptr<Protocol> protocol_;
};

} // namespace Client
} // namespace Dai

#endif // DAI_CLIENT_STRUCTURE_SYNCHRONIZER_H
