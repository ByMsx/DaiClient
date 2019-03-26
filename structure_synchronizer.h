#ifndef DAI_CLIENT_STRUCTURE_SYNCHRONIZER_H
#define DAI_CLIENT_STRUCTURE_SYNCHRONIZER_H

#include <Helpz/net_protocol.h>

#include <Dai/project.h>
#include <plus/dai/structure_synchronizer.h>

namespace Dai {
namespace Client {

class Structure_Synchronizer : public QObject, public Dai::Structure_Synchronizer
{
    Q_OBJECT
public:
    Structure_Synchronizer();

    void set_project(Project* project);
    void set_protocol(Helpz::Network::Protocol* protocol);

    bool modified() const;
public slots:
    void modify_client_structure(uint32_t user_id, uint8_t structType, QIODevice* data_dev);
    void send_project_structure(uint8_t struct_type, uint8_t msg_id, QIODevice* data_dev);
private:
    void send_modify_response(const QByteArray &buffer) override;
    void add_checker_types(QDataStream& ds);
    void add_device_items(QDataStream& ds);
    void add_groups(QDataStream& ds);
    void add_codes(const QVector<uint32_t>& ids, QDataStream* ds);

    bool modified_;
    Project* prj_;
    Helpz::Network::Protocol* protocol_;
};

} // namespace Client
} // namespace Dai

#endif // DAI_CLIENT_STRUCTURE_SYNCHRONIZER_H
