#ifndef DAI_CLIENT_STRUCTURE_SYNCHRONIZER_H
#define DAI_CLIENT_STRUCTURE_SYNCHRONIZER_H

#include <Helpz/net_protocol.h>

#include <Dai/project.h>
#include <Dai/db/group_status_item.h>
#include <plus/dai/structure_synchronizer_base.h>

namespace Dai {
namespace Ver_2_2 {
namespace Client {

//using namespace Dai::Client;

class Protocol;

class Structure_Synchronizer : public QObject, public Structure_Synchronizer_Base
{
    Q_OBJECT
public:
    Structure_Synchronizer(Helpz::Database::Thread *db_thread);

    void set_project(Project* project);

signals:
    void client_modified(uint32_t user_id);
public slots:
    void set_protocol(std::shared_ptr<Ver_2_2::Client::Protocol> protocol = {});
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
} // namespace Ver_2_2
} // namespace Dai

#endif // DAI_CLIENT_STRUCTURE_SYNCHRONIZER_H
