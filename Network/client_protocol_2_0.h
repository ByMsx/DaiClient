#ifndef DAI_PROTOCOL_2_0_H
#define DAI_PROTOCOL_2_0_H

#include <Dai/project.h>

#include "log_sender.h"
#include "client_protocol.h"

namespace Dai {
namespace Client {

class Protocol_2_0 : public Protocol
{
    Q_OBJECT
public:
    Protocol_2_0(Worker* worker, const Authentication_Info &auth_info);

signals:
    void restart();
    void write_to_item(quint32 item_id, const QVariant& raw_data);
    bool set_mode(quint32 mode_id, quint32 group_id);
    void set_param_values(const ParamValuesPack& pack);
    void exec_script_command(const QString& script);

    bool modify_project(quint8 struct_type, QIODevice* data_dev);

private:
    void connect_worker_signals();

    void ready_write() override;
    void process_message(uint8_t msg_id, uint16_t cmd, QIODevice& data_dev) override;
    void process_answer_message(uint8_t msg_id, uint16_t cmd, QIODevice& data_dev) override;

    void start_authentication();
    void send_project_structure(uint8_t struct_type, uint8_t msg_id, QIODevice* data_dev);
    void add_checker_types(QDataStream& ds);
    void add_device_items(QDataStream& ds);
    void add_groups(QDataStream& ds);
    void add_codes(const QVector<uint32_t>& ids, QDataStream* ds);
    void send_version(uint8_t msg_id);
    void send_time_info(uint8_t msg_id);

    void send_mode(uint mode_id, quint32 group_id);
    void send_status_added(quint32 group_id, quint32 info_id, const QStringList& args);
    void send_status_removed(quint32 group_id, quint32 info_id);


    void sendParamValues(const ParamValuesPack& pack);

    Project* prj_;

    Log_Sender log_sender_;
};

} // namespace Client
} // namespace Dai

#endif // DAI_PROTOCOL_2_0_H
