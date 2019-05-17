#ifndef DAI_PROTOCOL_2_0_H
#define DAI_PROTOCOL_2_0_H

#include <Dai/project.h>

#include "log_sender.h"
#include "structure_synchronizer.h"
#include "client_protocol.h"

namespace Dai {
namespace Client {

class Protocol_2_0 : public Protocol
{
    Q_OBJECT
public:
    Protocol_2_0(Worker* worker, Structure_Synchronizer* structure_synchronizer, const Authentication_Info &auth_info);
    ~Protocol_2_0();

signals:
    void restart(uint32_t user_id);
    void write_to_item(uint32_t user_id, uint32_t item_id, const QVariant& raw_data);
    bool set_mode(uint32_t user_id, quint32 mode_id, quint32 group_id);
    void set_group_param_values(uint32_t user_id, const QVector<Group_Param_Value>& pack);
    void exec_script_command(uint32_t user_id, const QString& script, bool is_function, const QVariantList& arguments);

    void send_project_structure(uint8_t struct_type, uint8_t msg_id, QIODevice* data_dev, Helpz::Network::Protocol* protocol);
//    bool modify_project(uint32_t user_id, quint8 struct_type, QIODevice* data_dev);
private:
    void connect_worker_signals();

    void ready_write() override;
    void process_message(uint8_t msg_id, uint16_t cmd, QIODevice& data_dev) override;
    void process_answer_message(uint8_t msg_id, uint16_t cmd, QIODevice& data_dev) override;

    void parse_script_command(uint32_t user_id, const QString& script, QIODevice* data_dev);
    void process_item_file(QIODevice &data_dev);

    void start_authentication();
    void process_authentication(bool authorized, const QUuid &connection_id);
    void send_version(uint8_t msg_id);
    void send_time_info(uint8_t msg_id);

    void send_mode(uint32_t user_id, uint mode_id, quint32 group_id);
    void send_status_added(quint32 group_id, quint32 info_id, const QStringList& args, uint32_t);
    void send_status_removed(quint32 group_id, quint32 info_id, uint32_t);
    void send_group_param_values(uint32_t user_id, const QVector<Group_Param_Value>& pack);

    Project* prj_;

    Log_Sender log_sender_;
    Structure_Synchronizer* structure_sync_;
};

} // namespace Client
} // namespace Dai

#endif // DAI_PROTOCOL_2_0_H
